# Smart Camera 2.0 consumer integration

> Unreleased documentation. Publish with the next compatible Smart! release; this candidate cannot be paired with the current Smart! package on SMR.

Smart Camera is a standalone local-player view service. Your mod registers a friendly name and stable namespaced string view IDs, then supplies a world frame or an absolute camera pose. Registration is separate from rendering and from saved player overrides. Idle views remain discoverable; deactivate until a subject is ready and unregister on owner/world teardown.

The [full API guide](https://github.com/majormer/SmartCamera/blob/master/SMARTCAMERA_API_INTEGRATION.md) covers optional plugin/module dependencies, feature discovery, code examples, ownership, multiple views, settings ranges, save scope, lifecycle and migration. The [complete Space Elevator Camera plugin](https://github.com/majormer/SmartCamera/tree/master/docs/examples/SpaceElevatorCamera) is included as a tested source reference, with its own player-look controls and no SmartFoundations dependency.

Player-facing behavior and screenshots: [Smart Camera](Smart-Camera).

_Last updated:2026-10-13 · Smart Camera 2.0.0 (unreleased)_
