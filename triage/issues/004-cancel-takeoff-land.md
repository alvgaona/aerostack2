Title: Cancelling a take-off or landing puts the drone in emergency
Labels: bug, behavior, platform

## Summary

Stopping a take-off or a landing before it finishes is treated as a failure, and the drone is put in
the emergency state. Since the emergency state can't be cleared without a restart (#1034), an
ordinary cancel ends the session: the platform keeps sending stop commands and refuses everything
else.

Cancelling is a normal operator action, for example to stop a landing over an obstacle or to hold a
take-off halfway up. It should leave the drone hovering and under control, not in emergency.

## Where

Take-off and landing put the drone in emergency on any outcome other than success:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_behaviors/as2_behaviors_motion/takeoff_behavior/src/takeoff_behavior.cpp#L177-L189

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_behaviors/as2_behaviors_motion/land_behavior/src/land_behavior.cpp#L178-L195

A stop request ends the behavior as aborted, which takes that path:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_behaviors/as2_behavior/include/as2_behavior/__impl/behavior_server__impl.hpp#L228-L230

## Acceptance criteria

Tested in the multirotor simulator:

- [ ] Cancelling a take-off halfway up leaves the drone hovering at its current height, and the
      platform reports flying.
- [ ] Cancelling a landing halfway down leaves the drone hovering at its current height, and the
      platform reports flying.
- [ ] After either cancel, a go to or a new landing is accepted and completes.
- [ ] Cancelling a take-off before the drone leaves the ground leaves it landed and armed.

## Related

- #1034
