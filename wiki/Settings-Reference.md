# Settings Reference

Smart! settings are exposed through the Smart Settings Form and SML mod configuration assets. This page lists the current config fields from source.

> Screenshot placeholder: Smart Settings Form with each major section visible, or a collage of Belt, Pipe, Power, HUD, and Arrows sections.

## Belt Auto-Connect

| Setting field | Purpose |
|---------------|---------|
| `bAutoConnectEnabled` | Enable belt Auto-Connect |
| `bAutoConnectDistributors` | Auto-connect distributor-style logistics |
| `AutoConnectMode` | Belt Auto-Connect mode enum value |
| `BeltLevelMain` | Main belt tier setting |
| `BeltLevelToBuilding` | Belt tier for machine/building connections |
| `BeltRoutingMode` | `0=Default`, `1=Curve`, `2=Straight` |
| `bStackableBeltEnabled` | Enable stackable belt support behavior |

## Pipe Auto-Connect

| Setting field | Purpose |
|---------------|---------|
| `bPipeAutoConnectEnabled` | Enable pipe Auto-Connect |
| `PipeLevelMain` | Main pipe tier setting |
| `PipeLevelToBuilding` | Pipe tier for machine/building connections |
| `PipeRoutingMode` | `0=Auto`, `1=Auto2D`, `2=Straight`, `3=Curve`, `4=Noodle`, `5=HorizontalToVertical` |
| `PipeIndicator` | Pipe indicator/no-indicator style setting |

## Power Auto-Connect

| Setting field | Purpose |
|---------------|---------|
| `bPowerAutoConnectEnabled` | Enable power Auto-Connect |
| `PowerConnectMode` | Power connection mode enum value |
| `PowerConnectRange` | Power Auto-Connect range |
| `PowerConnectReserved` | Reserved pole connections |
| `PowerPoleMk1MaxConnections` | Mk1 pole max connection setting |
| `PowerPoleMk2MaxConnections` | Mk2 pole max connection setting |
| `PowerPoleMk3MaxConnections` | Mk3 pole max connection setting |
| `PowerPoleMk4MaxConnections` | Mk4 pole max connection setting |

## Extend

| Setting field | Purpose |
|---------------|---------|
| `bExtendEnabled` | Enable Extend feature |
| `bExtendPowerEnabled` | Include power poles/wiring when using Extend where supported |

## Scaling

| Setting field | Purpose |
|---------------|---------|
| `bAutoHoldOnGridChange` | Automatically lock hologram position after grid modification |

## Smart Panel

| Setting field | Purpose |
|---------------|---------|
| `bApplyImmediately` | Apply panel changes immediately without clicking Apply |

## HUD

| Setting field | Purpose |
|---------------|---------|
| `bShowHUD` | Show Smart HUD |
| `HUDScale` | HUD scale |
| `HUDPositionX` | Normalized X position; source default is `0.02` |
| `HUDPositionY` | Normalized Y position; source default is `0.25` |
| `HUDTheme` | `0=Default/FICSIT Orange`, `1=Dark`, `2=Classic`, `3=High Contrast`, `4=Minimal`, `5=Monochrome` |

## Arrows

| Setting field | Purpose |
|---------------|---------|
| `bShowArrows` | Show directional arrows by default |
| `bShowArrowOrbit` | Enable animated orbiting of directional arrows |
| `bShowArrowLabels` | Show X/Y/Z text labels on directional arrows |

## Verified From

- `Source/SmartFoundations/Public/Config/Smart_ConfigStruct.h`
- Unreal Editor assets under `/SmartFoundations/SmartFoundations/Config/`
- `docs/Features/SmartPanel/IMPL_SmartPanel_CurrentFlow.md`

