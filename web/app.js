import { DisplayStudioBle } from "./ble.js";

const $ = (selector) => document.querySelector(selector);
const elements = {
  connectionState: $("#connection-state"), connectButton: $("#connect-button"),
  disconnectButton: $("#disconnect-button"), deployButton: $("#deploy-button"),
  syncTimeButton: $("#sync-time-button"), rebootButton: $("#reboot-button"),
  factoryResetButton: $("#factory-reset-button"), preset: $("#preset"),
  timezone: $("#timezone"), refreshMinutes: $("#refresh-minutes"),
  accent: $("#accent"), lunarText: $("#lunar-text"),
  wifiSsid: $("#wifi-ssid"), wifiPassword: $("#wifi-password"),
  previewTime: $("#preview-time"), previewWeekday: $("#preview-weekday"),
  previewDate: $("#preview-date"), previewLunar: $("#preview-lunar"),
  previewTitle: $("#preview-title"), log: $("#log"),
  deviceName: $("#device-name"), deviceFamily: $("#device-family"),
  displayInfo: $("#display-info"), firmwareVersion: $("#firmware-version"),
  capabilities: $("#capabilities"), projectName: $("#project-name"),
  sceneList: $("#scene-list"), sceneCount: $("#scene-count"),
  sceneName: $("#scene-name"), sceneEnabled: $("#scene-enabled"),
  addSceneButton: $("#add-scene-button"), duplicateSceneButton: $("#duplicate-scene-button"),
  deleteSceneButton: $("#delete-scene-button"), setActiveSceneButton: $("#set-active-scene-button"),
  activeSceneLabel: $("#active-scene-label")
};

const clone = (value) => JSON.parse(JSON.stringify(value));
let selectedSceneId = "main";
let projectState = null;

function log(message, data = null) {
  const suffix = data ? ` ${JSON.stringify(data)}` : "";
  elements.log.textContent += `\n[${new Date().toLocaleTimeString()}] ${message}${suffix}`;
  elements.log.scrollTop = elements.log.scrollHeight;
}

function setConnected(connected) {
  elements.connectionState.textContent = connected ? "Đã kết nối" : "Chưa kết nối";
  elements.connectionState.className = `badge ${connected ? "online" : "offline"}`;
  elements.connectButton.disabled = connected;
  elements.disconnectButton.disabled = !connected;
  elements.deployButton.disabled = !connected;
  elements.rebootButton.disabled = !connected;
  elements.factoryResetButton.disabled = !connected;
}

function dateParts(timezone = elements.timezone.value) {
  const now = new Date();
  const options = { timeZone: timezone };
  return {
    weekday: new Intl.DateTimeFormat("en-US", { ...options, weekday: "long" }).format(now),
    date: new Intl.DateTimeFormat("en-GB", { ...options, day: "2-digit", month: "2-digit", year: "numeric" }).format(now),
    time: new Intl.DateTimeFormat("en-GB", { ...options, hour: "2-digit", minute: "2-digit", hour12: false }).format(now)
  };
}

function updatePreview() {
  const values = dateParts();
  const preset = elements.preset.value;
  elements.previewTime.textContent = values.time;
  elements.previewWeekday.textContent = values.weekday;
  elements.previewDate.textContent = values.date;
  elements.previewLunar.textContent = elements.lunarText.value;
  elements.previewTime.style.color = elements.accent.value === "red" ? "#d92d20" : "#101828";
  elements.previewTime.hidden = preset === "calendar";
  elements.previewWeekday.hidden = preset === "clock";
  elements.previewDate.hidden = preset === "clock";
  elements.previewLunar.hidden = preset === "clock" || !elements.lunarText.value;
  const scene = currentScene();
  elements.previewTitle.textContent = `Preview · ${scene?.name || "Scene"}`;
}

function timezoneOffsetMinutes(timezone = elements.timezone.value) {
  if (timezone === "UTC") return 0;
  if (timezone === "Asia/Ho_Chi_Minh") return 420;
  return -new Date().getTimezoneOffset();
}

function buildSceneDefinition() {
  const preset = elements.preset.value;
  const widgets = [];
  if (preset !== "calendar") widgets.push({ type: "clock", x: 8, y: 5, width: 234, height: 55, color: elements.accent.value, align: "center", fontSize: 44 });
  if (preset !== "clock") {
    widgets.push(
      { type: "weekday", x: 8, y: 62, width: 234, height: 24, color: "black", align: "center", fontSize: 18 },
      { type: "date", x: 8, y: 87, width: 234, height: 18, color: "black", align: "center", fontSize: 14 }
    );
    if (elements.lunarText.value) widgets.push({ type: "text", text: elements.lunarText.value, x: 8, y: 105, width: 234, height: 15, color: "black", align: "center", fontSize: 11 });
  }
  return {
    schema: "display-studio/scene-v1", preset,
    appearance: { accent: elements.accent.value },
    content: { lunarText: elements.lunarText.value },
    canvas: { width: 250, height: 122, colorMode: "bwr" },
    runtime: { timezone: elements.timezone.value, refreshMinutes: Number(elements.refreshMinutes.value) },
    widgets
  };
}

function buildConfig() {
  return {
    schema: "display-studio/device-config-v1",
    scene: buildSceneDefinition(),
    wifi: { ssid: elements.wifiSsid.value, password: elements.wifiPassword.value }
  };
}

function applyConfig(config = {}) {
  const scene = config.scene || {};
  elements.preset.value = scene.preset || "clock-calendar";
  elements.timezone.value = scene.runtime?.timezone || "Asia/Ho_Chi_Minh";
  elements.refreshMinutes.value = String(scene.runtime?.refreshMinutes || 5);
  elements.accent.value = scene.appearance?.accent || "red";
  elements.lunarText.value = scene.content?.lunarText || "";
  elements.wifiSsid.value = config.wifi?.ssid || "";
  elements.wifiPassword.value = config.wifi?.password || "";
  updatePreview();
}

function newScene(id, name, config = null) {
  return { id, name, enabled: true, config: config ? clone(config) : buildConfig() };
}

function initializeProject() {
  projectState = {
    schema: "display-studio/project-v1", projectVersion: 1,
    name: "My Display Project", activeSceneId: "main",
    metadata: { createdBy: "Display Studio Sprint 1.5" },
    scenes: [newScene("main", "Main Scene")]
  };
  selectedSceneId = "main";
  loadSelectedScene();
}

function currentScene() {
  return projectState?.scenes.find((scene) => scene.id === selectedSceneId) || null;
}

function saveSelectedScene() {
  const scene = currentScene();
  if (!scene) return;
  scene.name = elements.sceneName.value.trim() || scene.name;
  scene.enabled = elements.sceneEnabled.checked;
  scene.config = buildConfig();
  projectState.name = elements.projectName.value.trim() || "My Display Project";
  projectState.metadata.updatedAt = new Date().toISOString();
}

function loadSelectedScene() {
  const scene = currentScene();
  if (!scene) return;
  elements.projectName.value = projectState.name;
  elements.sceneName.value = scene.name;
  elements.sceneEnabled.checked = scene.enabled;
  applyConfig(scene.config);
  renderSceneList();
}

function renderSceneList() {
  elements.sceneList.replaceChildren();
  for (const scene of projectState.scenes) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `scene-item${scene.id === selectedSceneId ? " selected" : ""}${!scene.enabled ? " disabled-scene" : ""}`;
    button.innerHTML = `<span>${scene.name}</span><small>${scene.id === projectState.activeSceneId ? "ACTIVE" : scene.enabled ? "READY" : "OFF"}</small>`;
    button.addEventListener("click", () => selectScene(scene.id));
    elements.sceneList.append(button);
  }
  elements.sceneCount.textContent = `${projectState.scenes.length}/16`;
  const active = projectState.scenes.find((scene) => scene.id === projectState.activeSceneId);
  elements.activeSceneLabel.textContent = `Scene hoạt động: ${active?.name || "—"}`;
  elements.deleteSceneButton.disabled = projectState.scenes.length <= 1;
  elements.addSceneButton.disabled = projectState.scenes.length >= 16;
  elements.setActiveSceneButton.disabled = !currentScene()?.enabled || selectedSceneId === projectState.activeSceneId;
}

function selectScene(id) {
  if (id === selectedSceneId) return;
  saveSelectedScene();
  selectedSceneId = id;
  loadSelectedScene();
}

function uniqueId(prefix = "scene") {
  let index = projectState.scenes.length + 1;
  let id = `${prefix}-${index}`;
  while (projectState.scenes.some((scene) => scene.id === id)) id = `${prefix}-${++index}`;
  return id;
}

function buildProject() {
  saveSelectedScene();
  const active = projectState.scenes.find((scene) => scene.id === projectState.activeSceneId);
  if (!active?.enabled) throw new Error("Scene hoạt động phải được bật.");
  if (new Set(projectState.scenes.map((scene) => scene.id)).size !== projectState.scenes.length) throw new Error("Scene ID bị trùng.");
  return clone(projectState);
}

function loadProject(project) {
  if (!project?.scenes?.length) throw new Error("Project không có Scene.");
  projectState = clone(project);
  projectState.metadata ||= {};
  selectedSceneId = projectState.activeSceneId || projectState.scenes[0].id;
  if (!projectState.scenes.some((scene) => scene.id === selectedSceneId)) selectedSceneId = projectState.scenes[0].id;
  loadSelectedScene();
}

function showDeviceInfo(info) {
  elements.deviceName.textContent = info.deviceName || info.deviceId || "ESP32";
  elements.deviceFamily.textContent = info.deviceFamily || "unknown";
  elements.displayInfo.textContent = info.display ? `${info.display.width} × ${info.display.height} · ${info.display.colorMode}` : "—";
  elements.firmwareVersion.textContent = info.firmware || "—";
  elements.capabilities.textContent = (info.capabilities || []).join(", ") || "—";
}

const ble = new DisplayStudioBle((message) => {
  if (message.type === "transport_progress") {
    if (message.percent === 0 || message.percent === 100 || message.percent % 10 === 0) log(`BLE upload: ${message.percent}% (${message.sentBytes}/${message.totalBytes} bytes)`);
    return;
  }
  log("RX", message);
  if (message.type === "device_info") {
    showDeviceInfo(message);
    ble.send({ command: "get_project" }).catch((error) => log(`Get Project failed: ${error.message}`));
  }
  if (message.type === "stored_project" && message.found) {
    try { loadProject(message.project); log(`Stored Project loaded: ${message.project.name}`); }
    catch (error) { log(`Stored Project invalid: ${error.message}`); }
  }
  if (message.type === "transport_retry") log(`Retry BLE frame ${message.sequence}, attempt ${message.attempt}/${message.maxAttempts}`);
  if (message.type === "status") log(`${message.ok ? "OK" : "ERROR"}: ${message.message || "completed"}`);
}, () => { setConnected(false); log("Bluetooth disconnected."); });

elements.connectButton.addEventListener("click", async () => {
  try { log("Opening Bluetooth device chooser..."); const device = await ble.connect(); setConnected(true); log(`Connected: ${device.name || "ESP32"}`); await ble.send({ command: "get_info" }); }
  catch (error) { log(`Connect failed: ${error.message}`); }
});
elements.disconnectButton.addEventListener("click", () => ble.disconnect());

elements.addSceneButton.addEventListener("click", () => {
  saveSelectedScene();
  const id = uniqueId();
  projectState.scenes.push(newScene(id, `Scene ${projectState.scenes.length + 1}`, currentScene()?.config));
  selectedSceneId = id;
  loadSelectedScene();
  log(`Scene added: ${id}`);
});

elements.duplicateSceneButton.addEventListener("click", () => {
  saveSelectedScene();
  const source = currentScene();
  const id = uniqueId(source.id);
  projectState.scenes.push({ ...clone(source), id, name: `${source.name} Copy`, enabled: true });
  selectedSceneId = id;
  loadSelectedScene();
  log(`Scene duplicated: ${id}`);
});

elements.deleteSceneButton.addEventListener("click", () => {
  if (projectState.scenes.length <= 1) return;
  const removed = currentScene();
  projectState.scenes = projectState.scenes.filter((scene) => scene.id !== selectedSceneId);
  if (projectState.activeSceneId === removed.id) {
    const replacement = projectState.scenes.find((scene) => scene.enabled) || projectState.scenes[0];
    replacement.enabled = true;
    projectState.activeSceneId = replacement.id;
  }
  selectedSceneId = projectState.scenes[0].id;
  loadSelectedScene();
  log(`Scene deleted: ${removed.name}`);
});

elements.setActiveSceneButton.addEventListener("click", () => {
  saveSelectedScene();
  const scene = currentScene();
  if (!scene.enabled) { log("Scene disabled cannot be active."); return; }
  projectState.activeSceneId = scene.id;
  renderSceneList();
  log(`Active Scene: ${scene.name}`);
});

elements.sceneName.addEventListener("input", () => { const scene = currentScene(); if (scene) { scene.name = elements.sceneName.value || scene.name; renderSceneList(); updatePreview(); } });
elements.sceneEnabled.addEventListener("change", () => {
  const scene = currentScene();
  if (scene.id === projectState.activeSceneId && !elements.sceneEnabled.checked) {
    elements.sceneEnabled.checked = true;
    log("Scene hoạt động không thể bị tắt. Hãy chọn Scene khác trước.");
    return;
  }
  scene.enabled = elements.sceneEnabled.checked;
  renderSceneList();
});

elements.syncTimeButton.addEventListener("click", async () => {
  try { await ble.request({ command: "set_time", epochMs: Date.now(), timezone: elements.timezone.value, timezoneOffsetMinutes: timezoneOffsetMinutes() }); log("Current browser time synchronized."); }
  catch (error) { log(`Time sync failed: ${error.message}`); }
});

elements.deployButton.addEventListener("click", async () => {
  try {
    const project = buildProject();
    const active = project.scenes.find((scene) => scene.id === project.activeSceneId);
    const runtime = active.config.scene.runtime;
    log("Sending Project...", { name: project.name, scenes: project.scenes.length, activeSceneId: project.activeSceneId });
    log("Step 1/3: Installing Project...");
    await ble.request({ command: "set_project", project }, 45000);
    log("Project installed and verified.");
    log("Step 2/3: Synchronizing time...");
    await ble.request({ command: "set_time", epochMs: Date.now(), timezone: runtime.timezone, timezoneOffsetMinutes: timezoneOffsetMinutes(runtime.timezone) });
    log("Time synchronized.");
    log("Step 3/3: Rendering active Scene...");
    await ble.request({ command: "apply" }, 30000);
    log("Deploy completed. Project verified, stored and rendered.");
  } catch (error) { log(`Deploy failed: ${error.message}`); }
});

elements.rebootButton.addEventListener("click", async () => { try { await ble.request({ command: "reboot" }); } catch (error) { log(`Reboot failed: ${error.message}`); } });
elements.factoryResetButton.addEventListener("click", async () => { if (!confirm("Xóa toàn bộ Project và cài đặt trên ESP32?")) return; try { await ble.request({ command: "factory_reset" }); } catch (error) { log(`Factory reset failed: ${error.message}`); } });

$("#save-local-button").addEventListener("click", () => { try { localStorage.setItem("display-studio-project-v1", JSON.stringify(buildProject())); log("Project saved in browser."); } catch (error) { log(`Save failed: ${error.message}`); } });
$("#load-local-button").addEventListener("click", () => { try { const raw = localStorage.getItem("display-studio-project-v1") || localStorage.getItem("display-studio-scene-v1"); if (!raw) throw new Error("No local Project found."); loadProject(JSON.parse(raw)); log("Local Project loaded."); } catch (error) { log(`Load failed: ${error.message}`); } });
$("#clear-log-button").addEventListener("click", () => { elements.log.textContent = "Log cleared."; });

for (const id of ["preset", "timezone", "refresh-minutes", "accent", "lunar-text", "wifi-ssid", "wifi-password"]) {
  $(`#${id}`).addEventListener("input", () => { saveSelectedScene(); updatePreview(); });
}

elements.projectName.addEventListener("input", () => { projectState.name = elements.projectName.value; });
setConnected(false);
initializeProject();
setInterval(updatePreview, 15000);
