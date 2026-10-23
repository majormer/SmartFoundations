# Water Extractors

Water Extractors support horizontal Smart grids, Extend and Restore. Their destinations must remain valid water placements.

## Scale or Extend a row

Select a Water Extractor and use X/Y grid counts and spacing for a horizontal layout. To Extend, hold the matching extractor and aim at an existing one, then inspect the copied extractor and supported pipe connections before building. See [Extend](Extend) for direction and captured-module controls.

Every child uses the water-depth and terrain checks already applied by Smart scaling. A valid first extractor does not make the other destinations valid. Moving the row onto shallow water or terrain can invalidate it.

![A horizontal two-extractor Smart preview beside its Scaling controls, with vertical repetition disabled.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/water-horizontal-grid.png)

This **2×1×1** preview repeats two Water Extractors horizontally in actual water. Both destinations passed placement validation; **Grid Z** and **Spacing Z** are disabled. The blue previews are unbuilt, and no pipe manifold is shown. Power daisy-chaining is enabled in this example.

## Connect the outlets

After placing the horizontal extractor grid, align a matching row of Pipeline Junction Crosses with its outlets. With Pipe Auto-Connect enabled, inspect the short branches and shared pipeline before building. Keep the entire route clear of the shore and rocks. Normal placement rules and material costs apply.

![Two horizontally repeated Water Extractors with short Auto-Connect branches into one shared pipeline.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/water-connected-manifold.png)

Two horizontally repeated Water Extractors with short Auto-Connect branches into one shared pipeline.

This example shows the connected layout. Supply power to run the extractors.

## Choose the controls

For a horizontal grid, open **K → Scaling** and enter **Grid X/Y** and **Spacing X/Y**, or use the [wheel and numpad scaling controls](Controls). The runtime HUD reports the grid and active spacing while the world preview shows where every extractor will go. Vertical grid controls do not enable another water layer.

For Extend, aim at a compatible existing extractor with the matching recipe equipped, then adjust the captured row with the Panel or world controls. For a saved Module, use the Restore workflow below. In every case, inspect each destination and its pipe routing before firing the build gun.

## Restore a saved arrangement

Capture the supported Extend layout as a Module, then use **K → Restore → Modules → Load Module** to preview it again. Recheck every destination and the full material cost; Restore still validates the water placements.

Water Extractors remain horizontal: vertical grids and vertical Extend/Restore layers are excluded. Other resource extractors do not gain free-placement scaling from this support. Miners can instead use [Smart Upgrade](Smart-Upgrade) on their original resource nodes.

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
