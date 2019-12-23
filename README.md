# Checkpoint

[![GitHub release](https://img.shields.io/github/release/allejo/Checkpoint.svg)](https://github.com/allejo/Checkpoint/releases/latest)
![Minimum BZFlag Version](https://img.shields.io/badge/BZFlag-v2.4.0+-blue.svg)
[![License](https://img.shields.io/github/license/allejo/Checkpoint.svg)](LICENSE.md)

This is a rewrite of the [original Checkpoint](https://forums.bzflag.org/viewtopic.php?f=79&t=16273&p=158192) plug-in that is used on obstacle course like maps to allow players to save their spawn positions.

## Requirements

- C++11
- BZFlag 2.4.20+

This plug-in follows [my standard instructions for compiling plug-ins](https://github.com/allejo/docs.allejo.io/wiki/BZFlag-Plug-in-Distribution).

## Usage

### Loading the plug-in

This plug-in does not take any configuration options at load time.

```
-loadplugin Checkpoint
```

### Custom BZDB Variables

These custom BZDB variables can be configured with `-set` in configuration files and may be changed at any time in-game by using the `/set` command.

```
-set <name> <value>
```

| Name | Type | Default | Description |
| ---- | ---- | ------- | ----------- |
| `_checkPointsLifetime` | int | 5 | Number of minutes a checkpoint record should be saved between rejoins for a player. |
| `_clearCheckPointsOnCap` | bool | false | Whether or not to clear all checkpoints after a flag capture. |

### Custom Slash Commands

| Command | Permission | Description |
| ------- | ---------- | ----------- |
| `/checkpoints <sub command>` | spawn | Slash command to interact with saved checkpoints. |
| `/cp <sub command>` | spawn | An alias for the `checkpoints` command. |

- `/checkpoints list` - List the 10 most recent checkpoints a player has reached
- `/checkpoints save` - Save the current position the tank is in within a given checkpoint to serve as the exact spawn position for the next spawn
- `/checkpoints swap <checkpoint name>` - Change your most recent checkpoint to respawn at a different location; you may only spawn at checkpoints you've reached before

### Custom Map Objects

This plug-in introduces the `CHECKPOINT` map object which supports the traditional `position`, `size`, and `rotation` attributes for rectangular objects and `position`, `height`, and `radius` for cylindrical objects.

```text
checkpoint
  name <name>
  position 0 0 0
  size 5 5 5
  rotation 0
  team <team>
  message <message>
end
```

- `name` - A unique name to reference this zone; this is a required field
- `message` (optional) - A message to send to the players when they have successfully entered the zone and have saved it
- `team` (optional) - This option restricts the checkpoint to only be available to tanks of a certain color. If this option is left out, it'll apply to all teams. Supported values:
  - 0 - Rogue Team
  - 1 - Red Team
  - 2 - Green Team
  - 3 - Blue Team
  - 4 - Purple Team
  - 5 - Rabbit Team
  - 6 - Hunter Team

## License

[MIT](LICENSE.md)
