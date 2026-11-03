Title: Emergency methods suggest actions that don't happen
Labels: bug, python-api, documentation

## Summary

The drone interface in the Python API has five emergency methods. Three of them send alerts that
nothing acts on, but their names, docstrings and log messages say otherwise:

| Method | What a user expects | What happens |
| --- | --- | --- |
| `send_emergency_land()` | The drone lands. It logs "Starting emergency landing." | Nothing |
| `send_emergency_hover()` | The drone hovers | Nothing |
| `send_emergency_land_to_aircraft()` | The flight controller lands the drone | Nothing |

The names also make the difference between the plain and the `_to_aircraft` versions hard to see.

## Where

The three methods:

https://github.com/aerostack2/aerostack2/blob/6d5f7c80c8e0b81397342caf2a0d8bb0267a4e73/as2_python_api/as2_python_api/drone_interface_base.py#L292-L303

## Acceptance criteria

- [ ] Until the alerts are handled, each of the three methods says in its docstring that the drone
      won't act on it yet, and logs a warning when called.
- [ ] No log message claims an action that doesn't happen. "Starting emergency landing." is gone.
- [ ] Once the alerts are handled, the docstrings describe what the drone actually does.
- [ ] The docs explain when to use each method: the `_to_aircraft` versions hand control to the
      flight controller and end the mission, while the others keep Aerostack2 in control.
## Related

- #1030
- #1031
