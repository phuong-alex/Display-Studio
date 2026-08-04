# Changelog

## Sprint 1.5.1

- Paused automatic Scene refresh while a BLE Studio session is connected.
- Preserved explicit `apply` rendering after Project deploy.
- Prevented endless 30-second e-ink retries after a BUSY failure.
- Marked each automatic refresh bucket before rendering, so a failed refresh is deferred until the next configured interval.
- Fixed BLE frame 0 ACK starvation caused by repeated blocking display refreshes.

## Sprint 1.5

- Added a multi-Scene Project manager to the web Studio.
- Added create, rename, duplicate, delete, enable and active Scene controls.
- Added Project name editing and complete multi-Scene browser persistence.
- Added loading of complete multi-Scene Projects from ESP32.
- Preserved Project schema V1 and BLE protocol 3.

## Sprint 1.3

- Moved command execution out of the BLE write callback.
- Added FreeRTOS queues for complete commands, transport ACKs and JSON responses.
- Added a paced TX pump that sends one notification chunk at a time.
- Added deferred reboot so the response is transmitted before restart.
- Added repeated-final-frame ACK handling so a lost final ACK can be retried safely.
- Preserved Project schema V1, request IDs and the existing browser frame format.

## Sprint 1.2.3

- Added binary framed BLE writes.
- Added per-frame ACK and stop-and-wait flow control.
- Added duplicate-frame detection and safe retransmission.
- Added four-attempt retry policy.
- Removed continuous GATT write burst that failed near 90 percent.
