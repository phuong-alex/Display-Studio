export const BLE = Object.freeze({
  service:
    "7fb90001-6f3e-4e65-9c7a-f13827b6a001",
  rx:
    "7fb90002-6f3e-4e65-9c7a-f13827b6a001",
  tx:
    "7fb90003-6f3e-4e65-9c7a-f13827b6a001"
});

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const BLE_FRAME_SIZE = 20;
const BLE_FRAME_HEADER = 3;
const BLE_FRAME_DATA =
  BLE_FRAME_SIZE - BLE_FRAME_HEADER;
const BLE_DATA_MARKER = 0xA5;
const BLE_ACK_MARKER = 0xA6;
const CHUNK_ACK_TIMEOUT_MS = 1500;
const CHUNK_MAX_ATTEMPTS = 4;

function sleep(milliseconds) {
  return new Promise(
    (resolve) => setTimeout(resolve, milliseconds)
  );
}

export class DisplayStudioBle {
  constructor(onMessage, onDisconnect) {
    this.onMessage = onMessage;
    this.onDisconnect = onDisconnect;
    this.device = null;
    this.server = null;
    this.rx = null;
    this.tx = null;
    this.receiveBuffer = "";
    this.pending = new Map();
    this.requestSequence = 0;
    this.sendChain = Promise.resolve();
    this.chunkAckWaiters = new Map();
  }

  get connected() {
    return Boolean(
      this.device?.gatt?.connected &&
      this.rx &&
      this.tx
    );
  }

  async connect() {
    if (!navigator.bluetooth) {
      throw new Error(
        "Trình duyệt không hỗ trợ Web Bluetooth. " +
        "Hãy dùng Chrome hoặc Edge trên máy tính."
      );
    }

    this.device =
      await navigator.bluetooth.requestDevice({
        filters: [
          {
            namePrefix: "DisplayStudio"
          }
        ],
        optionalServices: [BLE.service]
      });

    this.device.addEventListener(
      "gattserverdisconnected",
      () => this.handleDisconnect()
    );

    this.server = await this.device.gatt.connect();
    const service =
      await this.server.getPrimaryService(BLE.service);

    this.rx = await service.getCharacteristic(BLE.rx);
    this.tx = await service.getCharacteristic(BLE.tx);

    await this.tx.startNotifications();
    this.tx.addEventListener(
      "characteristicvaluechanged",
      (event) => this.handleNotification(event)
    );

    return this.device;
  }

  handleDisconnect() {
    this.rx = null;
    this.tx = null;
    this.receiveBuffer = "";

    for (const pending of this.pending.values()) {
      clearTimeout(pending.timeout);
      pending.reject(
        new Error("Bluetooth disconnected")
      );
    }

    this.pending.clear();

    for (
      const waiter of this.chunkAckWaiters.values()
    ) {
      clearTimeout(waiter.timeout);
      waiter.reject(
        new Error("Bluetooth disconnected")
      );
    }

    this.chunkAckWaiters.clear();
    this.sendChain = Promise.resolve();
    this.chunkAckWaiters = new Map();
    this.onDisconnect?.();
  }

  disconnect() {
    this.device?.gatt?.disconnect();
  }

  handleNotification(event) {
    const view = event.target.value;

    if (
      view.byteLength === 3 &&
      view.getUint8(0) === BLE_ACK_MARKER
    ) {
      const sequence = view.getUint16(
        1,
        true
      );

      const waiter =
        this.chunkAckWaiters.get(sequence);

      if (waiter) {
        clearTimeout(waiter.timeout);
        this.chunkAckWaiters.delete(sequence);
        waiter.resolve();
      }

      return;
    }

    this.receiveBuffer += decoder.decode(
      view,
      { stream: true }
    );

    while (this.receiveBuffer.includes("\n")) {
      const index = this.receiveBuffer.indexOf("\n");
      const line = this.receiveBuffer
        .slice(0, index)
        .trim();

      this.receiveBuffer =
        this.receiveBuffer.slice(index + 1);

      if (!line) continue;

      try {
        const message = JSON.parse(line);

        if (
          message.type === "status" &&
          message.requestId &&
          this.pending.has(message.requestId)
        ) {
          const pending =
            this.pending.get(message.requestId);

          clearTimeout(pending.timeout);
          this.pending.delete(message.requestId);

          if (message.ok) {
            pending.resolve(message);
          } else {
            pending.reject(
              new Error(
                message.message ||
                "device_command_failed"
              )
            );
          }
        }

        this.onMessage?.(message);
      } catch {
        this.onMessage?.({
          type: "raw",
          value: line
        });
      }
    }
  }

  async request(message, timeoutMs = 30000) {
    const requestId =
      `req-${Date.now()}-${++this.requestSequence}`;

    let timeout;

    const response = new Promise(
      (resolve, reject) => {
        timeout = setTimeout(() => {
          this.pending.delete(requestId);
          reject(
            new Error(
              `Timeout waiting for ${message.command}`
            )
          );
        }, timeoutMs);

        this.pending.set(requestId, {
          resolve,
          reject,
          timeout
        });
      }
    );

    try {
      await this.send({
        ...message,
        requestId
      });
    } catch (error) {
      clearTimeout(timeout);
      this.pending.delete(requestId);
      throw error;
    }

    return response;
  }

  async send(message) {
    const task = this.sendChain.then(
      () => this.sendNow(message)
    );

    this.sendChain = task.catch(() => {});
    return task;
  }

  waitForChunkAck(sequence) {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.chunkAckWaiters.delete(sequence);
        reject(
          new Error(
            `Chunk ACK timeout: ${sequence}`
          )
        );
      }, CHUNK_ACK_TIMEOUT_MS);

      this.chunkAckWaiters.set(sequence, {
        resolve,
        reject,
        timeout
      });
    });
  }

  async sendFrame(frame, sequence) {
    let lastError = null;

    for (
      let attempt = 1;
      attempt <= CHUNK_MAX_ATTEMPTS;
      attempt++
    ) {
      if (!this.connected) {
        throw new Error("Bluetooth disconnected");
      }

      const ack = this.waitForChunkAck(sequence);

      try {
        if (this.rx.writeValueWithoutResponse) {
          await this.rx.writeValueWithoutResponse(
            frame
          );
        } else {
          await this.rx.writeValue(frame);
        }

        await ack;
        return;
      } catch (error) {
        const waiter =
          this.chunkAckWaiters.get(sequence);

        if (waiter) {
          clearTimeout(waiter.timeout);
          this.chunkAckWaiters.delete(sequence);
        }

        lastError = error;

        this.onMessage?.({
          type: "transport_retry",
          sequence,
          attempt,
          maxAttempts: CHUNK_MAX_ATTEMPTS
        });

        await sleep(80 * attempt);
      }
    }

    throw lastError || new Error(
      `Unable to send chunk ${sequence}`
    );
  }

  async sendNow(message) {
    if (!this.connected) {
      throw new Error("Thiết bị chưa kết nối.");
    }

    const bytes = encoder.encode(
      JSON.stringify(message) + "\n"
    );

    const totalChunks = Math.ceil(
      bytes.length / BLE_FRAME_DATA
    );

    this.onMessage?.({
      type: "transport_progress",
      direction: "tx",
      sentBytes: 0,
      totalBytes: bytes.length,
      percent: 0
    });

    for (
      let offset = 0, sequence = 0;
      offset < bytes.length;
      offset += BLE_FRAME_DATA, sequence++
    ) {
      const payload = bytes.slice(
        offset,
        offset + BLE_FRAME_DATA
      );

      const frame = new Uint8Array(
        BLE_FRAME_HEADER + payload.length
      );

      frame[0] = BLE_DATA_MARKER;
      frame[1] = sequence & 0xff;
      frame[2] = (sequence >> 8) & 0xff;
      frame.set(payload, BLE_FRAME_HEADER);

      await this.sendFrame(frame, sequence);

      const sentBytes = Math.min(
        offset + payload.length,
        bytes.length
      );

      this.onMessage?.({
        type: "transport_progress",
        direction: "tx",
        chunk: sequence + 1,
        chunks: totalChunks,
        sentBytes,
        totalBytes: bytes.length,
        percent: Math.round(
          sentBytes * 100 / bytes.length
        )
      });
    }
  }
}
