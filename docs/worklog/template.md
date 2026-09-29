# *Feature Name*

*What the feature does and why, in a paragraph or two, from the point of view of
the code that uses it. For a change with nothing to see, say so: "There is no
change in behavior."*

## Behavior

*What code using the library sees: new or changed APIs, what they do, and edge
cases. Say if a public API changes in a way that breaks existing callers. Leave
this section out if nothing changes.*

## Design

*How it is built, by library, lower tiers first. Use the subsections that fit,
such as the ones below.*

### Names

*Each new name, and anything existing it might be confused with.*

### *Component*

```
// A sketch of the interface: the public methods, and anything a caller
// overrides.
```

- *Where it lives, and why it belongs in that library and tier.*
- *How it behaves, including threading and lifetime.*
- *Performance: what it costs, and when.*

**Brittleness:** *what a caller has to remember to get this right (paired
calls, state kept in sync, ordering), and how the design enforces it. Call out
anything that is left.*

### To confirm

*Facts the design depends on that nobody has checked yet, and which CL checks
each one. Record the findings here as they come in, and adjust the later CLs
before starting them.*

## CLs

*One library per CL where possible, lower tiers first. The status is `[ ]` not
started, `[~]` in progress (set by the CL's first edit), or `[x]` submitted
(set in the CL's own commit).*

### CL1 [ ] *library*: *what it does*

Depends on: nothing.

- *What changes, by file or class.*
- *"Unused, so no visible change." if nothing uses it yet.*

**Verify**
- Standard checks (see CLAUDE.md).
- *Unit tests: what they cover.*
- *Anything checked by hand in an example, and what should happen.*
- *Performance: what was measured, and the result.*

### CL2 [ ] *library*: *what it does*

Depends on: CL1.

- *...*

**Verify**
- Standard checks.
- *...*
