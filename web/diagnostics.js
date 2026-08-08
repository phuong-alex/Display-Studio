import { DisplayStudioBle } from "./ble.js";

const decoder = new TextDecoder();
const buffers = new WeakMap();
let activeBle = null;
let latestInfo = null;
let refreshTimer = null;
let refreshBlocked = false;
let resumeTimer = null;

const DIAGNOSTICS_INTERVAL_MS = 15000;
const POST_RENDER_QUIET_MS = 40000;

const originalConnect = DisplayStudioBle.prototype.connect;
DisplayStudioBle.prototype.connect = async function (...args) {
  const device = await originalConnect.apply(this, args);
  activeBle = this;
  refreshBlocked = false;
  scheduleRefresh(true);
  return device;
};

const originalDisconnect = DisplayStudioBle.prototype.disconnect;
DisplayStudioBle.prototype.disconnect = function (...args) {
  if (activeBle === this) activeBle = null;
  clearResumeTimer();
  refreshBlocked = false;
  scheduleRefresh(false);
  setStatus("Chưa kết nối", "offline");
  return originalDisconnect.apply(this, args);
};

const originalHandleDisconnect = DisplayStudioBle.prototype.handleDisconnect;
DisplayStudioBle.prototype.handleDisconnect = function (...args) {
  if (activeBle === this) activeBle = null;
  clearResumeTimer();
  refreshBlocked = false;
  scheduleRefresh(false);
  setStatus("Chưa kết nối", "offline");
  return originalHandleDisconnect.apply(this, args);
};

// Deploy and display refresh share the same BLE/CPU resources as Diagnostics.
// Pause background polling from upload_begin until the physical e-paper refresh
// has had enough quiet time to complete. `apply` only queues render and returns
// before the display worker is finished, so resuming immediately after its ACK
// would still interfere with the BUSY wait window.
const originalRequest = DisplayStudioBle.prototype.request;
DisplayStudioBle.prototype.request = async function (message, timeoutMs) {
  const command = message?.command || "";

  if (command === "upload_begin") {
    blockRefresh("Deploy đang chạy");
  }

  try {
    return await originalRequest.call(this, message, timeoutMs);
  } finally {
    if (command === "upload_abort") {
      resumeRefreshSoon(1200);
    } else if (command === "apply") {
      resumeRefreshSoon(POST_RENDER_QUIET_MS);
    }
  }
};

const originalHandleNotification = DisplayStudioBle.prototype.handleNotification;
DisplayStudioBle.prototype.handleNotification = function (event) {
  observeNotification(this, event);
  return originalHandleNotification.call(this, event);
};

function $(selector) {
  return document.querySelector(selector);
}

function clearResumeTimer() {
  if (resumeTimer) clearTimeout(resumeTimer);
  resumeTimer = null;
}

function blockRefresh(reason = "Tạm dừng") {
  clearResumeTimer();
  refreshBlocked = true;
  const updated = $("#diag-updated");
  if (updated) updated.textContent = `${reason} · Diagnostics tạm dừng`;
}

function resumeRefreshSoon(delayMs = 0) {
  clearResumeTimer();
  resumeTimer = setTimeout(() => {
    resumeTimer = null;
    refreshBlocked = false;
    if (activeBle?.connected) refresh();
  }, delayMs);
}

function formatBytes(value) {
  const bytes = Number(value || 0);
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
}

function formatUptime(milliseconds) {
  let seconds = Math.floor(Number(milliseconds || 0) / 1000);
  const days = Math.floor(seconds / 86400);
  seconds %= 86400;
  const hours = Math.floor(seconds / 3600);
  seconds %= 3600;
  const minutes = Math.floor(seconds / 60);
  seconds %= 60;
  const clock = [hours, minutes, seconds].map((value) => String(value).padStart(2, "0")).join(":");
  return days ? `${days}d ${clock}` : clock;
}

function setText(id, value) {
  const element = document.getElementById(id);
  if (element) element.textContent = value ?? "—";
}

function setStatus(text, state) {
  const element = $("#diagnostics-health");
  if (!element) return;
  element.textContent = text;
  element.className = `badge ${state}`;
}

function healthFor(info) {
  if (!info) return { text: "Chưa có dữ liệu", state: "neutral" };
  const free = Number(info.heap?.free || 0);
  const largest = Number(info.heap?.largest || 0);
  const fsFree = Number(info.storage?.free || 0);
  const fsTotal = Number(info.storage?.total || 0);
  const fsRatio = fsTotal ? fsFree / fsTotal : 0;

  if (free < 60000 || largest < 45000 || (fsTotal && fsRatio < 0.08)) {
    return { text: "Critical", state: "critical" };
  }
  if (free < 100000 || largest < 80000 || (fsTotal && fsRatio < 0.2)) {
    return { text: "Warning", state: "warning" };
  }
  return { text: "Healthy", state: "online" };
}

function render(info) {
  latestInfo = info;
  const health = healthFor(info);
  setStatus(health.text, health.state);

  setText("diag-firmware", info.firmware);
  setText("diag-protocol", info.protocolVersion ?? info.protocol);
  setText("diag-uptime", formatUptime(info.uptimeMs ?? info.uptime));

  setText("diag-heap-free", formatBytes(info.heap?.free));
  setText("diag-heap-largest", formatBytes(info.heap?.largest));
  setText("diag-heap-min", formatBytes(info.heap?.minimum));

  setText("diag-fs-total", formatBytes(info.storage?.total));
  setText("diag-fs-used", formatBytes(info.storage?.used));
  setText("diag-fs-free", formatBytes(info.storage?.free));

  setText("diag-project-size", formatBytes(info.project?.bytes ?? info.storage?.projectBytes));
  setText("diag-project-scenes", info.project?.scenes ?? "—");
  setText("diag-active-scene", info.project?.activeSceneId || "—");

  const updated = $("#diag-updated");
  if (updated) updated.textContent = `Cập nhật: ${new Date().toLocaleTimeString()}`;
}

function observeNotification(instance, event) {
  const view = event.target.value;
  if (view.byteLength === 3 && view.getUint8(0) === 0xA6) return;

  let buffer = buffers.get(instance) || "";
  buffer += decoder.decode(view, { stream: true });

  while (buffer.includes("\n")) {
    const index = buffer.indexOf("\n");
    const line = buffer.slice(0, index).trim();
    buffer = buffer.slice(index + 1);
    if (!line) continue;
    try {
      const message = JSON.parse(line);
      if (message.type === "runtime_info") render(message);
    } catch {
      // The main BLE transport owns error reporting. Diagnostics stays passive.
    }
  }

  buffers.set(instance, buffer);
}

async function refresh() {
  const button = $("#diagnostics-refresh");
  if (!activeBle?.connected) {
    setStatus("Chưa kết nối", "offline");
    return;
  }

  if (refreshBlocked) return;

  if (button) button.disabled = true;
  try {
    await activeBle.send({ command: "get_runtime_info" });
  } catch (error) {
    setStatus("Không đọc được", "critical");
  } finally {
    if (button) setTimeout(() => { button.disabled = false; }, 500);
  }
}

function scheduleRefresh(enabled) {
  if (refreshTimer) clearInterval(refreshTimer);
  refreshTimer = null;
  if (enabled) {
    setTimeout(refresh, 800);
    refreshTimer = setInterval(refresh, DIAGNOSTICS_INTERVAL_MS);
  }
}

async function copyDiagnostics() {
  if (!latestInfo) {
    await refresh();
    return;
  }

  const text = [
    "Display Studio Diagnostics",
    `Firmware: ${latestInfo.firmware || "—"}`,
    `Protocol: ${latestInfo.protocolVersion ?? latestInfo.protocol ?? "—"}`,
    `Uptime: ${formatUptime(latestInfo.uptimeMs ?? latestInfo.uptime)}`,
    `Heap free: ${latestInfo.heap?.free ?? "—"} bytes`,
    `Heap largest: ${latestInfo.heap?.largest ?? "—"} bytes`,
    `Heap minimum: ${latestInfo.heap?.minimum ?? "—"} bytes`,
    `LittleFS total: ${latestInfo.storage?.total ?? "—"} bytes`,
    `LittleFS used: ${latestInfo.storage?.used ?? "—"} bytes`,
    `LittleFS free: ${latestInfo.storage?.free ?? "—"} bytes`,
    `Project: ${latestInfo.project?.bytes ?? latestInfo.storage?.projectBytes ?? "—"} bytes`,
    `Scenes: ${latestInfo.project?.scenes ?? "—"}`,
    `Active scene: ${latestInfo.project?.activeSceneId || "—"}`
  ].join("\n");

  try {
    await navigator.clipboard.writeText(text);
    const button = $("#diagnostics-copy");
    if (button) {
      const original = button.textContent;
      button.textContent = "Đã copy";
      setTimeout(() => { button.textContent = original; }, 1200);
    }
  } catch {
    console.log(text);
  }
}

window.addEventListener("DOMContentLoaded", () => {
  $("#diagnostics-refresh")?.addEventListener("click", refresh);
  $("#diagnostics-copy")?.addEventListener("click", copyDiagnostics);
  setStatus("Chưa kết nối", "offline");
});
