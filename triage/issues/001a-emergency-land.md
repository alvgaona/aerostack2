Title: Emergency land alert is ignored, the drone keeps flying
Labels: bug, platform, core, security

## Summary

Aerostack2 lets an operator or a safety node send an **emergency land** alert to a drone. Today the
drone ignores it: no landing starts, there's no error, and the drone keeps doing what it was doing.
The two other platform alerts, **kill switch** and **emergency hover**, work.

## Current behavior

- An `EMERGENCY_LAND` alert on `alert_event` is received and dropped. Nothing is logged.
- The Python API's `send_emergency_land_to_aircraft()` sends this alert, so it has no effect either.

## Where

The message says negative alerts are handled by the platform:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_msgs/msg/AlertEvent.msg#L3-L6

The platform handles kill switch and emergency hover, and emergency land falls through to `default`:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/aerial_platform.cpp#L348-L361

## Acceptance criteria

Tested in the multirotor simulator and in Gazebo, flying at 3 m during a go to:

- [ ] An emergency land alert starts a descent within 1 s, and the drone lands within 0.5 m of where
      the alert was received.
- [ ] The landing is done by the drone's own flight controller and still completes when the motion
      controller, behaviors and state estimator are stopped.
- [ ] The running go to and any other Aerostack2 command are ignored until the drone has landed.
- [ ] A kill switch sent halfway down stops the motors.
- [ ] An emergency hover sent halfway down stops the descent, and an emergency land sent afterwards
      resumes it.
- [ ] The alert repeated at 10 Hz throughout doesn't restart or disturb the landing.
- [ ] After touchdown the motors are disarmed and the platform status shows landed.
- [ ] A platform that can't land on its own holds position instead of cutting the motors, and logs and
      reports that emergency land isn't supported.
- [ ] Each platform's documentation states whether it supports emergency land.
## Related

- #1031
- #1032
- #1034
