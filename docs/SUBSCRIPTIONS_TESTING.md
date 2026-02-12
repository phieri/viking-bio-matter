# Phase 5: Subscriptions & Attribute Reporting Testing Guide

## Overview
This guide explains how to test the subscription and reporting functionality implemented in Phase 5 using chip-tool.

## Prerequisites
- Viking Bio Matter Bridge firmware with Phase 5 subscriptions support
- chip-tool installed and configured
- Device commissioned to a Matter fabric

## Testing Subscription Functionality

### 1. Subscribe to Flame State (OnOff Cluster)
Subscribe to flame state with minimum interval of 1 second and maximum interval of 10 seconds:

```bash
chip-tool onoff subscribe on-off 1 10 0x1234 1
```

Expected behavior:
- Device responds with SubscribeResponse containing subscription_id
- When flame state changes, device sends ReportData within 1-10 seconds
- Reports continue every 10 seconds (max_interval) even if no change

### 2. Subscribe to Fan Speed (LevelControl Cluster)
Subscribe to fan speed percentage:

```bash
chip-tool levelcontrol subscribe current-level 1 10 0x1234 1
```

Expected behavior:
- Subscription established successfully
- Reports sent when fan speed changes (respecting min_interval)
- Periodic reports sent every max_interval

### 3. Subscribe to Temperature (TemperatureMeasurement Cluster)
Subscribe to temperature readings (in centidegrees):

```bash
chip-tool temperaturemeasurement subscribe measured-value 1 10 0x1234 1
```

Expected behavior:
- Temperature changes trigger reports
- Value is in centidegrees (e.g., 2500 = 25.00°C)

### 4. Subscribe to Multiple Attributes Simultaneously
Establish multiple subscriptions in different terminals:

Terminal 1:
```bash
chip-tool onoff subscribe on-off 1 10 0x1234 1
```

Terminal 2:
```bash
chip-tool levelcontrol subscribe current-level 1 10 0x1234 1
```

Terminal 3:
```bash
chip-tool temperaturemeasurement subscribe measured-value 1 10 0x1234 1
```

Expected behavior:
- All subscriptions work independently
- Each subscription tracks its own min/max intervals
- Maximum of 10 concurrent subscriptions supported

## Observing Reports

### Using Viking Bio Simulator
To trigger attribute changes and observe reports:

```bash
# In one terminal, run chip-tool subscriptions
chip-tool onoff subscribe on-off 1 10 0x1234 1

# In another terminal, use the simulator to send data
python3 examples/viking_bio_simulator.py /dev/ttyUSB0 -i 2.0
```

This will send Viking Bio data every 2 seconds, triggering attribute updates and subscription reports.

### Expected Report Format
ReportData messages should contain:
- SubscriptionId (matches ID from SubscribeResponse)
- AttributeReports list with:
  - AttributePath (endpoint, cluster, attribute)
  - Data (current value)

## Verification Checklist

- [ ] SubscribeRequest is parsed correctly
- [ ] SubscribeResponse includes valid subscription_id
- [ ] Attribute changes trigger reports (respecting min_interval)
- [ ] Periodic reports sent at max_interval
- [ ] Multiple subscriptions work simultaneously
- [ ] Subscription limit (10) is enforced
- [ ] Session closure removes all subscriptions for that session
- [ ] ReportData format matches Matter specification

## Debugging

### Check Serial Output
Monitor the device serial console to see:
```
Matter: Attribute changed (EP:1, CL:0x0006, AT:0x0000) = true
Matter: Subscription notification sent
```

### Verify Subscription State
The firmware tracks:
- Active subscriptions count (max 10)
- Last report time per subscription
- Min/max intervals per subscription

### Common Issues

1. **Reports not received**
   - Check min_interval hasn't been violated
   - Verify attribute is actually changing
   - Confirm subscription is still active

2. **Too many reports**
   - Attribute may be changing rapidly
   - Consider increasing min_interval

3. **Subscription failed**
   - May have reached 10 subscription limit
   - Check session is still active
   - Verify cluster and attribute are supported

## Performance Characteristics

- **Subscription overhead**: ~50 bytes per subscription
- **Report generation**: <1ms per report
- **Maximum subscriptions**: 10 concurrent
- **Memory usage**: ~500 bytes for subscription manager
- **Min interval precision**: 1 second
- **Max interval precision**: 1 second

## Binary Size Impact

Phase 5 additions:
- subscribe_handler: ~4 KB
- report_generator: ~3 KB
- subscription_bridge: ~1 KB
- Total: ~8 KB incremental

Total firmware with Phase 1-5: ~100 KB (well within 2MB flash budget)
