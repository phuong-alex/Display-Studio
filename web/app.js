import { DisplayStudioBle } from "./ble.js";

const elements = {
  connectionState:
    document.querySelector("#connection-state"),
  connectButton:
    document.querySelector("#connect-button"),
  disconnectButton:
    document.querySelector("#disconnect-button"),
  deployButton:
    document.querySelector("#deploy-button"),
  syncTimeButton:
    document.querySelector("#sync-time-button"),
  rebootButton:
    document.querySelector("#reboot-button"),
  factoryResetButton:
    document.querySelector("#factory-reset-button"),
  preset:
    document.querySelector("#preset"),
  timezone:
    document.querySelector("#timezone"),
  refreshMinutes:
    document.querySelector("#refresh-minutes"),
  accent:
    document.querySelector("#accent"),
  lunarText:
    document.querySelector("#lunar-text"),
  wifiSsid:
    document.querySelector("#wifi-ssid"),
  wifiPassword:
    document.querySelector("#wifi-password"),
  preview:
    document.querySelector("#preview"),
  previewTime:
    document.querySelector("#preview-time"),
  previewWeekday:
    document.querySelector("#preview-weekday"),
  previewDate:
    document.querySelector("#preview-date"),
  previewLunar:
    document.querySelector("#preview-lunar"),
  log:
    document.querySelector("#log"),
  deviceName:
    document.querySelector("#device-name"),
  deviceFamily:
    document.querySelector("#device-family"),
  displayInfo:
    document.querySelector("#display-info"),
  firmwareVersion:
    document.querySelector("#firmware-version"),
  capabilities:
    document.querySelector("#capabilities")
};

let deviceInfo = null;

function log(message, data = null) {
  const stamp = new Date().toLocaleTimeString();
  const suffix = data
    ? ` ${JSON.stringify(data)}`
    : "";

  elements.log.textContent +=
    `\n[${stamp}] ${message}${suffix}`;

  elements.log.scrollTop =
    elements.log.scrollHeight;
}

function setConnected(connected) {
  elements.connectionState.textContent =
    connected ? "Đã kết nối" : "Chưa kết nối";

  elements.connectionState.className =
    `badge ${connected ? "online" : "offline"}`;

  elements.connectButton.disabled = connected;
  elements.disconnectButton.disabled = !connected;
  elements.deployButton.disabled = !connected;
  elements.rebootButton.disabled = true;
  elements.factoryResetButton.disabled = true;
}

function dateParts() {
  const now = new Date();

  const weekday = new Intl.DateTimeFormat(
    "en-US",
    {
      weekday: "long",
      timeZone: elements.timezone.value
    }
  ).format(now);

  const date = new Intl.DateTimeFormat(
    "en-GB",
    {
      day: "2-digit",
      month: "2-digit",
      year: "numeric",
      timeZone: elements.timezone.value
    }
  ).format(now);

  const time = new Intl.DateTimeFormat(
    "en-GB",
    {
      hour: "2-digit",
      minute: "2-digit",
      hour12: false,
      timeZone: elements.timezone.value
    }
  ).format(now);

  return { time, weekday, date };
}

function updatePreview() {
  const values = dateParts();
  const preset = elements.preset.value;

  elements.previewTime.textContent = values.time;
  elements.previewWeekday.textContent =
    values.weekday;
  elements.previewDate.textContent = values.date;
  elements.previewLunar.textContent =
    elements.lunarText.value;

  elements.previewTime.style.color =
    elements.accent.value === "red"
      ? "#d92d20"
      : "#101828";

  elements.previewTime.hidden =
    preset === "calendar";

  elements.previewWeekday.hidden =
    preset === "clock";

  elements.previewDate.hidden =
    preset === "clock";

  elements.previewLunar.hidden =
    preset === "clock" ||
    !elements.lunarText.value;
}

function timezoneOffsetMinutes() {
  if (elements.timezone.value === "UTC") {
    return 0;
  }

  if (
    elements.timezone.value ===
    "Asia/Ho_Chi_Minh"
  ) {
    return 420;
  }

  return -new Date().getTimezoneOffset();
}

function buildScene() {
  const preset = elements.preset.value;
  const widgets = [];

  if (preset !== "calendar") {
    widgets.push({
      type: "clock",
      x: 8,
      y: 5,
      width: 234,
      height: 55,
      color: elements.accent.value,
      align: "center",
      fontSize: 44
    });
  }

  if (preset !== "clock") {
    widgets.push(
      {
        type: "weekday",
        x: 8,
        y: 62,
        width: 234,
        height: 24,
        color: "black",
        align: "center",
        fontSize: 18
      },
      {
        type: "date",
        x: 8,
        y: 87,
        width: 234,
        height: 18,
        color: "black",
        align: "center",
        fontSize: 14
      }
    );

    if (elements.lunarText.value) {
      widgets.push({
        type: "text",
        text: elements.lunarText.value,
        x: 8,
        y: 105,
        width: 234,
        height: 15,
        color: "black",
        align: "center",
        fontSize: 11
      });
    }
  }

  return {
    schema: "display-studio/scene-v1",
    preset,
    appearance: {
      accent: elements.accent.value
    },
    content: {
      lunarText: elements.lunarText.value
    },
    canvas: {
      width: 250,
      height: 122,
      colorMode: "bwr"
    },
    runtime: {
      timezone: elements.timezone.value,
      refreshMinutes:
        Number(elements.refreshMinutes.value)
    },
    widgets
  };
}

function buildLegacyConfig() {
  return {
    schema: "display-studio/device-config-v1",
    scene: buildScene(),
    wifi: {
      ssid: elements.wifiSsid.value,
      password: elements.wifiPassword.value
    }
  };
}

function buildProject() {
  return {
    schema: "display-studio/project-v1",
    projectVersion: 1,
    name: "My Display Project",
    activeSceneId: "main",
    metadata: {
      createdBy: "Display Studio Sprint 1.2.2",
      updatedAt: new Date().toISOString()
    },
    scenes: [
      {
        id: "main",
        name: "Main Scene",
        enabled: true,
        config: buildLegacyConfig()
      }
    ]
  };
}

function applyConfigToForm(config) {
  const scene = config?.scene || {};
  const runtime = scene.runtime || {};

  elements.preset.value =
    scene.preset || "clock-calendar";

  elements.timezone.value =
    runtime.timezone || "Asia/Ho_Chi_Minh";

  elements.refreshMinutes.value =
    String(runtime.refreshMinutes || 5);

  elements.accent.value =
    scene.appearance?.accent || "red";

  elements.lunarText.value =
    scene.content?.lunarText || "";

  updatePreview();
}

function showDeviceInfo(info) {
  deviceInfo = info;

  elements.deviceName.textContent =
    info.deviceName || info.deviceId || "ESP32";

  elements.deviceFamily.textContent =
    info.deviceFamily || "unknown";

  elements.displayInfo.textContent =
    info.display
      ? `${info.display.width} × ` +
        `${info.display.height} · ` +
        `${info.display.colorMode}`
      : "—";

  elements.firmwareVersion.textContent =
    info.firmware || "—";

  elements.capabilities.textContent =
    (info.capabilities || []).join(", ") || "—";
}

const ble = new DisplayStudioBle(
  (message) => {
    if (message.type === "transport_progress") {
      if (
        message.percent === 0 ||
        message.percent === 100 ||
        message.percent % 10 === 0
      ) {
        log(
          `BLE upload: ${message.percent}% ` +
          `(${message.sentBytes}/${message.totalBytes} bytes)`
        );
      }
      return;
    }

    log("RX", message);

    if (message.type === "device_info") {
      showDeviceInfo(message);
      elements.rebootButton.disabled = false;
      elements.factoryResetButton.disabled = false;

      ble.send({ command: "get_project" }).catch(
        (error) =>
          log(`Get Project failed: ${error.message}`)
      );
    }

    if (
      message.type === "stored_project" &&
      message.found
    ) {
      const activeScene =
        message.project.scenes.find(
          (scene) =>
            scene.id ===
            message.project.activeSceneId
        );

      if (activeScene) {
        applyConfigToForm(activeScene.config);
      }

      log(
        `Stored Project loaded: ` +
        `${message.project.name}`
      );
    }

    if (
      message.type === "stored_config" &&
      message.found
    ) {
      applyConfigToForm(message.config);
      log(
        "Compatibility Scene loaded from ESP32."
      );
    }

    if (message.type === "transport_retry") {
      log(
        `Retry BLE frame ${message.sequence}, ` +
        `attempt ${message.attempt}/${message.maxAttempts}`
      );
    }

    if (message.type === "status") {
      if (message.ok) {
        log(`OK: ${message.message || "completed"}`);
      } else {
        log(`ERROR: ${message.message || "failed"}`);
      }
    }
  },
  () => {
    setConnected(false);
    log("Bluetooth disconnected.");
  }
);

elements.connectButton.addEventListener(
  "click",
  async () => {
    try {
      log("Opening Bluetooth device chooser...");
      const device = await ble.connect();
      setConnected(true);
      log(`Connected: ${device.name || "ESP32"}`);
      await ble.send({ command: "get_info" });
    } catch (error) {
      log(`Connect failed: ${error.message}`);
    }
  }
);

elements.disconnectButton.addEventListener(
  "click",
  () => ble.disconnect()
);

elements.syncTimeButton.addEventListener(
  "click",
  async () => {
    try {
      await ble.request({
        command: "set_time",
        epochMs: Date.now(),
        timezone:
          elements.timezone.value,
        timezoneOffsetMinutes:
          timezoneOffsetMinutes()
      });

      log("Current browser time synchronized.");
    } catch (error) {
      log(`Time sync failed: ${error.message}`);
    }
  }
);

elements.deployButton.addEventListener(
  "click",
  async () => {
    try {
      const project = buildProject();
      const scene =
        project.scenes[0].config.scene;

      log("Sending Project...", {
        name: project.name,
        scenes: project.scenes.length,
        preset: scene.preset,
        refreshMinutes:
          scene.runtime.refreshMinutes
      });

      log("Step 1/3: Installing Project...");

      await ble.request(
        {
          command: "set_project",
          project
        },
        30000
      );

      log("Project installed and verified.");

      log("Step 2/3: Synchronizing time...");

      await ble.request({
        command: "set_time",
        epochMs: Date.now(),
        timezone:
          elements.timezone.value,
        timezoneOffsetMinutes:
          timezoneOffsetMinutes()
      });

      log("Time synchronized.");

      log("Step 3/3: Rendering active Scene...");

      await ble.request(
        {
          command: "apply"
        },
        30000
      );

      log(
        "Deploy completed. Project verified, stored and rendered."
      );
    } catch (error) {
      log(`Deploy failed: ${error.message}`);
    }
  }
);

elements.rebootButton.addEventListener(
  "click",
  async () => {
    try {
      await ble.request({ command: "reboot" });
    } catch (error) {
      log(`Reboot failed: ${error.message}`);
    }
  }
);

elements.factoryResetButton.addEventListener(
  "click",
  async () => {
    const confirmed = confirm(
      "Xóa toàn bộ Scene, Wi-Fi và cài đặt trên ESP32?"
    );

    if (!confirmed) return;

    try {
      await ble.request({ command: "factory_reset" });
    } catch (error) {
      log(`Factory reset failed: ${error.message}`);
    }
  }
);

document
  .querySelector("#save-local-button")
  .addEventListener("click", () => {
    localStorage.setItem(
      "display-studio-scene-v1",
      JSON.stringify(buildProject())
    );
    log("Project saved in browser.");
  });

document
  .querySelector("#load-local-button")
  .addEventListener("click", () => {
    const stored = localStorage.getItem(
      "display-studio-scene-v1"
    );

    if (!stored) {
      log("No local Scene found.");
      return;
    }

    const storedValue = JSON.parse(stored);

    const config =
      storedValue.schema ===
      "display-studio/project-v1"
        ? storedValue.scenes.find(
            (scene) =>
              scene.id ===
              storedValue.activeSceneId
          )?.config
        : storedValue;

    if (!config) {
      log("Stored Project has no active Scene.");
      return;
    }

    const runtime =
      config.scene?.runtime || {};

    elements.timezone.value =
      runtime.timezone || "Asia/Ho_Chi_Minh";

    elements.refreshMinutes.value =
      String(runtime.refreshMinutes || 5);

    elements.wifiSsid.value =
      config.wifi?.ssid || "";

    elements.wifiPassword.value =
      config.wifi?.password || "";

    applyConfigToForm(config);

    log("Local Project loaded.");
    updatePreview();
  });

document
  .querySelector("#clear-log-button")
  .addEventListener("click", () => {
    elements.log.textContent = "Log cleared.";
  });

for (const id of [
  "preset",
  "timezone",
  "refresh-minutes",
  "accent",
  "lunar-text"
]) {
  document
    .querySelector(`#${id}`)
    .addEventListener("input", updatePreview);
}

setConnected(false);
updatePreview();
setInterval(updatePreview, 15000);
