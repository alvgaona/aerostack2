Title: A successful take-off or landing with the platform method logs an error
Labels: bug, behavior, platform

## Summary

When a drone takes off or lands with the platform's own method (`takeoff_plugin_platform` or
`land_plugin_platform`), the "took off" or "landed" event is reported twice: once by the platform and
once by the behavior. The second one is rejected, so every successful take-off logs
"Could not set FSM to Took OFF", and every successful landing logs "Could not set FSM to Landed".

The drone behaves correctly, but operators see an error after a normal flight and learn to ignore
errors from these behaviors.

## Where

The platform reports the event when its own take-off or landing succeeds:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/aerial_platform.cpp#L318-L338

The behavior reports it again on success:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_behaviors/as2_behaviors_motion/takeoff_behavior/src/takeoff_behavior.cpp#L179-L182

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_behaviors/as2_behaviors_motion/land_behavior/src/land_behavior.cpp#L180-L184

## Acceptance criteria

Tested in the multirotor simulator with the platform take-off and land methods:

- [ ] A successful take-off logs no error, and the platform reports flying.
- [ ] A successful landing logs no error, and the platform reports landed.
- [ ] The other take-off and land methods (position, speed, trajectory) still end in flying and
      landed, with no error.
