# Superb Input Precision

## What does it do?

Superb Input Precision lets your inputs register at a higher precision than vanilla's discrete ticks, while staying as close to vanilla physics as possible. Vanilla normally processes queued inputs for the tick all at once. This mod tracks where in the tick they should be, so your clicks land when you actually want them to.

## Why did I make this mod?

Click Between Steps and Click Between Frames both use a method called **tick splitting** to achieve subtick inputs. When an input lands between two ticks, the tick gets split into two or more smaller subticks that run independently.
(Click Between *Steps* is slightly different as it allows no more than one split right in the middle. When referring to "vanilla" in the example below, it is assumed that Click Between Steps is disabled.)

The problem is that tick splitting doesn't reproduce vanilla physics exactly for gravity based modes. For example, a cube at 1x speed falling under gravity at 240 TPS:

- Gravity per tick: `0.958199 * (54/240) = 0.215594775` yvels/tick
- Starting velocity: 0 yvels

**Vanilla (one full tick):**
`v = 0 - 0.215594775 = -0.215594775`
`dy = 0.225 * (-0.215594775) =` **-0.048508824375 units**

**Tick split at 30% (two subticks of 0.3 and 0.7):**
**Subtick 1 (30%):**
`v = 0 - (0.215594775 * 0.3) = -0.0646784325`
`dy = 0.225 * 0.3 * (-0.0646784325) = -0.00436579419375`
**Subtick 2 (70%):**
`v = -0.0646784325 - (0.215594775 * 0.7) = -0.215594775`
`dy = 0.225 * 0.7 * (-0.215594775) = -0.0339561770625`
**Total dy =** **-0.03832197125625 units**

The velocity ends up the same, but the position doesn't. This comes from gravity being applied to velocity before displacement in each subtick. Splitting one tick into two smaller ones changes the intermediate velocity, which changes the total displacement. The error varies with split position and can accumulate. (Note: in actuality, velocity is rounded to 3 decimal places. For the sake of "simplicity" I used the full non-rounded values here. In game the disparity may be even worse, since every subtick gets rounded separately.)

Tick splitting also runs collision checks and other per-tick logic in the middle of a tick, which vanilla never does.

## How it works

Superb Input Precision takes a different approach. Vanilla still owns the physics and runs exactly one normal tick at a time. Inputs are still processed at the start of the tick like vanilla, and then the mod corrects the result.

- **Y position correction:** vanilla runs the input at the start of the tick, so the player moves as if the input had already happened for the whole tick. For the part of the tick before the input, you should still have been moving with the pre-input state, so your Y position is adjusted by that difference. For inputs that set your velocity (like a cube jump), that means moving with your old velocity instead of the new jump velocity up until the input time. For inputs that change acceleration (like holding or releasing in ship), the velocity carries through, so the difference comes from using the pre-input *gravity* up until the input time. With several inputs in one tick, each one adds its own piece of the correction.

- **Y velocity correction:** gravity should only act on each velocity for the part of the tick it actually existed. For inputs that set and override your velocity (like a cube jump), the gravity before the input is thrown away and only the remaining part of the tick counts. For inputs that change acceleration (like holding or releasing in ship), the tick gets a mix of the old and new gravity. The velocity is adjusted once to automatically account for either scenario depending on the input. Like the Y position correction, several inputs in a tick are each handled, in order.

Note: that corrected velocity is a value vanilla could never produce, since it only ever applies gravity in whole ticks. Any method that allows subtick input precision (tick splitting included) has to end up at that same velocity, because that's what an input partway through a tick means. The difference is that this mod gets there without running extra physics ticks. When Velocity Unrounding is off, the corrected velocity is still rounded to 3 decimal places like any vanilla velocity.

Inputs fire at the most recent input sample (rate is customizable in mod settings) or immediately (if Instantaneous Inputs is enabled), and the corrections handle where the player should be.

**Wave is the one exception:** because wave velocity is constant between inputs (no acceleration), tick splitting doesn't cause the displacement error above. So for wave, the tick is split at the input point, which is the most accurate (and simplest) option. Every other gamemode uses the corrections instead. This does mean that unlike vanilla, wave gets collision checks in between ticks.

The result is subtick input precision that aims to stay as faithful to vanilla physics as possible.

## Future plans

- Platformer keys support
- Botting support
- 2.1 mode (someday, maybe, i hope)

## Credits

- Thanks to syzzi for the original CBF idea, this was obviously heavily inspired by it. [CBF GitHub](https://github.com/theyareonit/Click-Between-Frames)

- Thanks to alex/sinc0s for logo inspo and permission

## Bugs

I'm not an expert modder or coder, so don't be surprised if you find any bugs. Please leave a [bug report](https://github.com/ch-zzzy/Superb-Input-Precision/issues/new?template=bug-report.yml) if you have any issues.
