# Minimal Matter Implementation - PR Roadmap

This document outlines the pull request strategy for implementing the minimal Matter protocol stack for Viking Bio Matter Bridge on Raspberry Pi Pico W.

## Strategy

Each phase is implemented as a separate PR to allow for:
- ✅ Incremental review and testing
- ✅ Clear git history with logical boundaries
- ✅ Ability to pause/resume development
- ✅ Independent validation of each layer

## Phase 1: TLV Codec Foundation
**Branch**: `feature/tlv-codec`  
**Size**: ~900 LOC  
**Duration**: 7-10 days  
**Status**: 🔜 Not Started

### Deliverables
- `src/matter_minimal/codec/tlv.h` - Public API
- `src/matter_minimal/codec/tlv.c` - Implementation
- `src/matter_minimal/codec/tlv_types.h` - Type definitions
- `tests/test_tlv.c` - Unit tests (runs on host PC)
- Documentation in `docs/phases/PHASE_1_TLV_CODEC.md`

### Success Criteria
- All unit tests pass
- Can encode/decode primitive types (int, uint, bool, string)
- Can encode/decode nested structures
- Test coverage > 90%
- Binary size < 15KB

### Dependencies
- None (foundational layer)

---

## Phase 2: Message Codec & UDP Transport
**Branch**: `feature/message-transport`  
**Size**: ~1500 LOC  
**Duration**: 5-7 days  
**Status**: ⏸️ Blocked by Phase 1

### Deliverables
- `src/matter_minimal/codec/message_codec.h/c` - Matter message encoding
- `src/matter_minimal/transport/udp_transport.h/c` - UDP socket handling
- Integration with lwIP (via existing network_adapter)
- Unit tests for message codec
- Integration tests for UDP send/receive

### Success Criteria
- Can encode/decode Matter message headers
- Can send/receive UDP packets on port 5540
- Message integrity validation works
- Binary size < 20KB

### Dependencies
- Phase 1 complete (TLV codec)
- Existing network_adapter.cpp (WiFi/lwIP)

---

## Phase 3: Security Layer (PASE & Sessions)
**Branch**: `feature/security`  
**Size**: ~2000 LOC  
**Duration**: 10-14 days  
**Status**: ⏸️ Blocked by Phase 2

### Deliverables
- `src/matter_minimal/security/pase.h/c` - PASE (SPAKE2+) implementation
- `src/matter_minimal/security/session_mgr.h/c` - Session management
- Integration with mbedTLS (via existing crypto_adapter)
- Unit tests for key derivation
- Integration tests for PASE handshake

### Success Criteria
- Can establish PASE session using setup PIN
- Can encrypt/decrypt messages with AES-128-CCM
- Session storage works (up to 5 sessions)
- Binary size < 25KB

### Dependencies
- Phase 2 complete (message codec)
- Existing crypto_adapter.cpp (mbedTLS wrappers)
- Existing storage_adapter.cpp (session persistence)

---

## Phase 4: Interaction Model (Read Handler)
**Branch**: `feature/read-handler`  
**Size**: ~1800 LOC  
**Duration**: 7-10 days  
**Status**: ⏸️ Blocked by Phase 3

### Deliverables
- `src/matter_minimal/interaction/read_handler.h/c` - ReadRequest/Response
- `src/matter_minimal/clusters/descriptor.h/c` - Descriptor cluster
- `src/matter_minimal/clusters/onoff.h/c` - OnOff cluster
- `src/matter_minimal/clusters/level_control.h/c` - LevelControl cluster
- `src/matter_minimal/clusters/temperature.h/c` - TemperatureMeasurement cluster
- Integration with existing matter_attributes system
- Tests with chip-tool

### Success Criteria
- Can respond to ReadRequest (secured)
- All 4 clusters implemented and readable
- Integration with matter_attributes works
- chip-tool can read flame/speed/temperature
- Binary size < 30KB

### Dependencies
- Phase 3 complete (security)
- Existing matter_attributes.cpp (attribute storage)

---

## Phase 5: Subscriptions & Reporting
**Branch**: `feature/subscriptions`  
**Size**: ~1200 LOC  
**Duration**: 7-10 days  
**Status**: ⏸️ Blocked by Phase 4

### Deliverables
- `src/matter_minimal/interaction/subscribe_handler.h/c` - SubscribeRequest/Response
- Subscription storage and management
- ReportData message generation
- Integration with matter_attributes_update()
- Tests with chip-tool subscriptions

### Success Criteria
- Can establish subscriptions
- Attribute changes trigger ReportData
- Subscription renewal works
- chip-tool receives real-time updates
- Binary size < 10KB (incremental)

### Dependencies
- Phase 4 complete (read handler)

---

## Phase 6: Commissioning & Polish
**Branch**: `feature/commissioning`  
**Size**: ~1000 LOC  
**Duration**: 10-14 days  
**Status**: ⏸️ Blocked by Phase 5

### Deliverables
- `src/matter_minimal/commissioning/network_commissioning.h/c`
- Commissioning flow implementation
- Fabric storage in flash
- QR code generation (integrate existing)
- Error handling improvements
- Logging and diagnostics
- Documentation updates
- Performance testing

### Success Criteria
- Can commission device via chip-tool
- Device survives reboot (fabric persistence)
- All integration tests pass
- Works with multiple controllers
- Documentation complete
- Binary size < 94KB total (all phases)

### Dependencies
- Phase 5 complete (subscriptions)
- Existing storage_adapter.cpp (fabric storage)
- Existing platform_manager.cpp (PIN derivation, QR code)

---

## Phase 7: Cleanup & SDK Removal
**Branch**: `feature/remove-sdk`  
**Size**: N/A (deletions)  
**Duration**: 1-2 days  
**Status**: ⏸️ Blocked by Phase 6

### Deliverables
- Remove `third_party/connectedhomeip/` directory
- Remove SDK references from `CMakeLists.txt`
- Remove old platform port files (if not reusable)
- Update `.gitignore`
- Update CI/CD workflows
- Update documentation (README, CONTRIBUTING, etc.)

### Success Criteria
- Build succeeds without SDK
- All tests pass
- Firmware size < 850KB
- CI pipeline green

### Dependencies
- Phase 6 complete (all features working)

---

## Total Project Timeline

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1: TLV Codec | 7-10 days | 10 days |
| Phase 2: Transport | 5-7 days | 17 days |
| Phase 3: Security | 10-14 days | 31 days |
| Phase 4: Read Handler | 7-10 days | 41 days |
| Phase 5: Subscriptions | 7-10 days | 51 days |
| Phase 6: Commissioning | 10-14 days | 65 days |
| Phase 7: Cleanup | 1-2 days | 67 days |

**Total Estimated Time**: 9-10 weeks (with 1 full-time developer)

---

## PR Checklist

Before submitting each PR, ensure:

- [ ] All unit tests pass
- [ ] Integration tests pass (if applicable)
- [ ] Code compiles without warnings (`-Wall -Wextra`)
- [ ] Binary size is within budget
- [ ] Documentation updated (README, API docs, copilot-instructions)
- [ ] CHANGELOG.md updated
- [ ] No credentials or secrets committed
- [ ] Code follows existing style
- [ ] Git history is clean (no WIP commits)

---

## Review Process

1. **Self-review**: Developer reviews own PR before submission
2. **Automated checks**: CI/CD pipeline runs (build, tests, size checks)
3. **Code review**: Maintainer reviews for correctness, style, architecture
4. **Testing**: Reviewer tests on hardware (if available)
5. **Approval**: Maintainer approves and merges to main

---

## Current Status

- [x] Architecture documented (`docs/MINIMAL_MATTER_ARCHITECTURE.md`)
- [x] Phase 1 plan documented (`docs/phases/PHASE_1_TLV_CODEC.md`)
- [ ] Phase 1 implementation in progress
- [ ] Phase 2-7 pending

**Next Action**: Begin Phase 1 implementation (TLV codec)

---

**Last Updated**: 2026-02-12 19:37:02  
**Document Owner**: TBD  
**Status**: Planning Complete, Implementation Starting
