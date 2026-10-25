// A box drawn from nine theme pictures (corners, stretched edges, middle),
// optionally with a pointer on the bottom edge. It fills its parent; the
// pictures reach beyond it by the given margins, for rim and shadow.
//
//   pieces         file prefix: PIECES-tl.png, -tm, -tr, -l, -c, -r, -bl,
//                  -bm, -br, and -bt for the bottom edge with the pointer
//   piece_size     width and height of a corner picture
//   margins        left, top, right, bottom
//   pointer_width  width of the -bt picture

import QtQuick

Item {
    id: box

    required property var looks
    required property string themeDirectory

    // The pointer tip's x from the box's left edge; negative for no pointer.
    property real pointerX: -1

    anchors.fill: parent

    readonly property real pieceSize: looks.piece_size
    // The pictures' outer edges, and where the corner pictures end.
    readonly property real outerLeft: -looks.margins[0]
    readonly property real outerTop: -looks.margins[1]
    readonly property real outerRight: width + looks.margins[2]
    readonly property real outerBottom: height + looks.margins[3]
    readonly property real innerLeft: outerLeft + pieceSize
    readonly property real innerTop: outerTop + pieceSize
    readonly property real innerRight: outerRight - pieceSize
    readonly property real innerBottom: outerBottom - pieceSize

    readonly property bool hasPointer: pointerX >= 0 && looks.pointer_width !== undefined
    readonly property real pointerLeft:
        hasPointer ? Math.round(pointerX - looks.pointer_width / 2) : innerRight
    readonly property real pointerRight: hasPointer ? pointerLeft + looks.pointer_width : innerRight

    component Piece: Image {
        required property string piece
        source: "file://" + box.themeDirectory + "/" + box.looks.pieces + "-" + piece + ".png"
        fillMode: Image.Stretch
    }

    Piece { piece: "tl"; x: box.outerLeft; y: box.outerTop; width: box.pieceSize; height: box.pieceSize }
    Piece { piece: "tm"; x: box.innerLeft; y: box.outerTop; width: box.innerRight - box.innerLeft; height: box.pieceSize }
    Piece { piece: "tr"; x: box.innerRight; y: box.outerTop; width: box.pieceSize; height: box.pieceSize }

    Piece { piece: "l"; x: box.outerLeft; y: box.innerTop; width: box.pieceSize; height: box.innerBottom - box.innerTop }
    Piece { piece: "c"; x: box.innerLeft; y: box.innerTop; width: box.innerRight - box.innerLeft; height: box.innerBottom - box.innerTop }
    Piece { piece: "r"; x: box.innerRight; y: box.innerTop; width: box.pieceSize; height: box.innerBottom - box.innerTop }

    Piece { piece: "bl"; x: box.outerLeft; y: box.innerBottom; width: box.pieceSize; height: box.pieceSize }
    Piece { piece: "bm"; x: box.innerLeft; y: box.innerBottom; width: box.pointerLeft - box.innerLeft; height: box.pieceSize }
    Piece { piece: "bt"; visible: box.hasPointer; x: box.pointerLeft; y: box.innerBottom; width: box.pointerRight - box.pointerLeft; height: box.pieceSize }
    Piece { piece: "bm"; visible: box.hasPointer; x: box.pointerRight; y: box.innerBottom; width: box.innerRight - box.pointerRight; height: box.pieceSize }
    Piece { piece: "br"; x: box.innerRight; y: box.innerBottom; width: box.pieceSize; height: box.pieceSize }
}
