# Superb Input Precision

## What does it do?

This mod lets your inputs register at a much finer resolution than vanilla's tick grid, while still aiming to keep the physics like vanilla. You get customizable input precision while staying true to vanilla physics. (See the GitHub README for an explanation of how CBF and CBS affect physics, and how this mod works.)

Please note: while the goal is vanilla-accurate physics, higher input precision itself is not vanilla and likely never will be unless RobTop updates Click Between Steps again. This mod may be considered cheating, so use at your own risk.

## Future plans

- Platformer support
- (optional) Sub-tick collision checks
- 2.1 mode (use Velocity Unrounding for now, 2.1-exclusive bugs not yet available)
- Botting support

## Credits

- Thanks to syzzi for the original CBF idea, this was obviously heavily inspired by it. [CBF Github](https://github.com/theyareonit/Click-Between-Frames)
- Thanks to Alphalaneous for reverse engineering PlayerObject::update(), the mod would not be available for non-windows without it. [hsaD yrtemoeG Github](https://github.com/Alphalaneous/hsaD-yrtemoeG)

## Bugs

I'm not an expert modder or coder, so don't be surprised if you find any bugs. Please leave a [bug report](https://github.com/ch-zzzy/Subtick-Inputs-API/issues/new?template=bug-report.yml) on the API's page if you have any issues. (Most of the code is there so any bugs are almost definitely not from Superb Input Precision.)
