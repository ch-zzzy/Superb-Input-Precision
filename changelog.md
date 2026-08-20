# Changelog

## v1.0.0-beta.4

- forgot something

## v1.0.0-beta.3

- (experimental) added functions for botting compatibility

## v1.0.0-beta.2

- force *enabled* click on steps such that inputs are not always processed on the first frame of the tick (caused issues when there is more than one tick in a frame, e.g. <240fps)

## v1.0.0-beta.1

- merged Subtick Inputs API with this mod, nothing functionally changed

---

## API v0.6.1

- rewrote getGravPerTick (this has been unreleased for like a week or two i forgot ✌️)
- fixed debug logs
- bump geode version

## SIP v0.1.4

- bump api ver
- let api handle cbs/cos disabling

## API v0.6.0

- reworked processInputs again, should hopefully fix the mod doing effectively nothing below 240fps (which kinda defeated the purpose)
- added a setting to force disable CBS+COS from within the API, so dependent mods don't need to handle it
- added isApiEnabled(), deprecated isApiDisabled()
- changed gravity precision to better match vanilla (i'll thoroughly check this someday to find other issues)
- lots of refactoring

## SIP v0.1.3

- changed processQueuedButtons hook priority
- bump geode version
- mark wave trail drag/draw fix as incompatiblities

## API v0.5.0

- restructured the input processing model
- added debug logging
- bump geode version

## SIP v0.1.2

- made Instantaneous Inputs on by default
- used config listeners

## API v0.4.1

- fixed listeners' respective events

## API v0.4.0

- added listeners for the configs similar to geode's listenForSettingChanges
- added updateRotation for tick splits, might fix some wave stuff, might not

## API v0.3.1

- added non-windows support, no velocity unrounding for those platforms for now

## SIP v0.1.1

- added non-windows support

## API v0.3.0

- replaced the y-displacement midhook with a full reimplementation of PlayerObject::update, potential for non-windows support 😮 (backup incase it goes wrong)
- fixed some wave stuff, added subtick collisions for them
- removed the SafetyHook dependency
- general refactoring

## API v0.2.0

- removed processInputs PlayerObject* param
- fixed p2 input doing nothing in non-dual mode

## API v0.1.3

- rounded getGravPerTick to 3dp if velocity unrounding is not enabled

## SIP v0.1.0

- public beta (or alpha? idrk), expect bugs

## API v0.1.2

- updated mod.json and README, and slightly simplified the midhook

## API v0.1.1

- added cheat tag

## API v0.1.0

- public beta (i've barely tested this at all lol)
