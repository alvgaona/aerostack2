Title: Force hover and force land alerts are ignored
Labels: bug, behavior, motion-controller, security

## Summary

Aerostack2 defines two alerts that the framework itself should act on: **force hover** and
**force land**. Unlike the emergency alerts, they keep the drone under Aerostack2's control and let
the mission continue or end cleanly. No component acts on either today. The drone keeps flying its
current task.

## Where

The message says positive alerts are handled by the framework:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_msgs/msg/AlertEvent.msg#L11-L13

The platform ignores them on purpose, and no other component subscribes to them:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/aerial_platform.cpp#L342-L344

Geozones send the configured alert when a drone leaves a geocage:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_utilities/as2_geozones/src/as2_geozones.cpp#L172-L175

## Acceptance criteria

Tested in the multirotor simulator with a Python mission, using the standard launch with nothing
extra started.

Force hover:

- [ ] During a go to, follow path, follow reference or trajectory, the drone stops and holds its
      position and heading within 1 s.
- [ ] The interrupted movement reports aborted to the mission.
- [ ] A new go to sent right after is accepted and completes.
- [ ] Sent during take-off, the drone holds at its current height.
- [ ] With the example geozones config, a drone flying out of its zone stops at the border.

Force land:

- [ ] During a follow path, the drone stops and lands at its current position using its configured
      landing method, and reports landed.
- [ ] A go to sent during the landing is refused. A kill switch or emergency alert still works.
- [ ] After landing, the drone can take off again normally.

Both:

- [ ] Sent while landed, the alert is ignored and an info message is logged.
- [ ] The alert repeated at 10 Hz doesn't restart or disturb the response.
- [ ] Each response is logged with the alert's description.
## Related

- #1030
- #1032
