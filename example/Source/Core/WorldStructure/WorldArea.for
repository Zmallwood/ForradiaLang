' Copyright (c) 2026 Andreas Åkerberg
' SPDX-License-Identifier: MIT

Import Tile

Class WorldArea
    List<List<Tile>> tiles

    New
        For x = 0 To 99
            tiles.Add(List<Tile>())

            For y = 0 To 99
                tiles[x].Add(Tile())
            Next
        Next
    End
End