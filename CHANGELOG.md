# Changelog

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
