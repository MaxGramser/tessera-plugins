# Changelog

## 0.1.2 - 2026-11-26

- New limit **VOC fine from**: where a VOC sensor's scale starts. It makes room for VOC sensors without a unit that
  count up from 0 for clean air, such as Amazon's Air Quality Monitor (0 to 100, higher is worse), next to the
  Sensirion index the tile reads them as. Set it to 0, then **VOC fine up to** in that sensor's own numbers. Nothing
  changes while the field stays empty.

## 0.1.1 - 2026-11-14

- The tile said it keeps 12 KB inside the chip, so a screen with a few other tiles refused it. It keeps next to nothing there: its parts live in the larger memory beside the chip. A screen now takes it as any other tile.

## 0.1.0 - 2026-11-14

- The first version. One tile for the air of a room: a verdict in the colour of the card, a ring with an arc for
  every sensor, a cell for each value on larger tiles, a card with all of them, and an explanation of each value
  with where it comes from, a first step and its last 24 hours. Eight kinds of room, limits of your own, and the
  sensors of one device filled in at once. Needs plugin API 0.26.
