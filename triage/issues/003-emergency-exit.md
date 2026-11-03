Title: The platform can't leave the emergency state without a restart
Labels: bug, platform, core

## Summary

Once a drone enters the emergency state, nothing can take it out of it. The platform keeps sending
stop commands on every cycle, and the only way to fly again is to restart the platform node.

Staying in emergency while the drone is armed is the right default. What's missing is a safe,
deliberate way to clear it once the drone is on the ground and disarmed. Without it, a drone that
lands after an emergency can't report that it has landed, and every emergency, including the ones
caused by an ordinary cancel, ends the session.

## Where

The state machine defines transitions into emergency from every state, and none out of it:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/platform_state_machine.cpp#L146-L166

While in emergency, the platform sends a stop command on every cycle:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_core/src/aerial_platform.cpp#L383-L385

## Acceptance criteria

- [ ] While the drone is in emergency and armed, it stays in emergency whatever command it receives.
- [ ] Once the drone is on the ground and disarmed, an operator can clear the emergency with an
      explicit request.
- [ ] A request to clear the emergency while armed is refused, with a message saying why.
- [ ] After clearing, the drone can arm, take off and fly again without restarting any node.
- [ ] Entering and clearing the emergency are both logged.
- [ ] The platform docs describe how to clear an emergency.

## Related

- #1030
