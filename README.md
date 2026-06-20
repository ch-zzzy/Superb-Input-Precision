# Superb Input Precision

## What does it do?

Superb Input Precision lets your inputs register at a much finer resolution than vanilla's discrete ticks, while keeping the physics faithful to vanilla. Vanilla normally processes queued inputs for the tick all at once. This mod tracks where in the tick they should be, so your clicks land when you actually want them to.

It's built on top of the [Subtick Inputs API](https://github.com/ch-zzzy/Subtick-Inputs-API), which handles the displacement correction.

## Why did I make this mod?

Click Between Steps and Click Between Frames both use a method called **tick splitting** to allow sub-tick inputs. When an input lands between two ticks, the tick gets split into two or more smaller ticks that run independently.
(Vanilla Click Between Steps is slightly different as it allows no more than one split right in the middle.)

The problem is that tick splitting doesn't reproduce vanilla physics exactly for gravity based modes. For example, a cube at 1x speed falling under gravity at 240 TPS:

- Gravity per tick: `0.958199 * (54/240) = 0.215594775` yvels/tick
- Starting velocity: 0 yvels

**Vanilla (one full tick):**
`v = 0 - 0.215594775 = -0.215594775`
`dy = 0.225 * (-0.215594775) =` **-0.048508824375 units**

**Tick split at 30% (two subticks of 0.3 and 0.7):**
**Step 1 (30%):**
`v = 0 - (0.215594775 * 0.3) = -0.0646784325`
`dy = 0.225 * 0.3 * (-0.0646784325) = -0.00436579419375`
**Step 2 (70%):**
`v = -0.0646784325 - (0.215594775 * 0.7) = -0.2158418575`
`dy = 0.225 * 0.7 * (-0.2158418575) = -0.03399509255625`
**Total dy = -0.03836088675 units**

Those clearly don't match. This comes from gravity being applied to velocity before displacement in each subtick. Splitting one tick into two smaller ones changes the intermediate velocity, which changes the total displacement. The error varies with split position and can accumulate. (Note: in actuality, velocity is rounded to 3 decimal places. For the sake of "simplicity" I used the full non-rounded values here. In game the disparity may be even worse due to the lost precision.)

## How it works

Because tick splitting introduces that error for accelerating gamemodes, Superb Input Precision takes a different approach. Vanilla still owns the physics and runs its normal ticks, but this mod (via the Subtick Inputs API) applies a Y-displacement correction so that an input applied partway through a tick produces the position vanilla would have produced if inputs were processed at that moment.

Inputs fire at the nearest input check (rate is customizable in mod settings) or immediately (if Instantaneous Inputs is enabled), and the correction handles where the player should be.

**Wave is the one exception:** because wave velocity is constant between inputs (no acceleration), tick splitting is exact for it because there's no intermediate-velocity error to introduce. So for wave, the tick is split at the input point, which is the most accurate (and simplest) option. Every other gamemode uses the displacement correction instead.

The result is sub-tick input precision that aims to stay faithful to vanilla physics.

## Future plans

- Platformer support
- (optional) Sub-tick collision checks
- 2.1 mode (use Velocity Unrounding for now, 2.1-exclusive bugs not yet available)
- Non-Windows support (I won't be working on that until TulipHook adds midhooking)
- Botting support

## Credits

Thanks to syzzi for the original CBF idea this was obviously heavily inspired by it.

## Bugs

I'm not an expert modder or coder, so don't be surprised if you find any bugs. Please leave a [bug report](https://github.com/ch-zzzy/Subtick-Inputs-API/issues/new) on the API's page if you have any issues. (Most of the code is there so any bugs are almost definitely not from Superb Input Precision.)
