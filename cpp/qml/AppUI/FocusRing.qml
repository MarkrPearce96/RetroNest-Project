import QtQuick

// FocusRing — a keyboard/controller focus ring for heterogeneous pages whose
// controls don't fit a plain list (GenericListPage) or card grid
// (GenericMultiCardPicker): e.g. a long scrollable dashboard mixing buttons,
// toggles, links, and Repeater-driven cards with display-only content.
//
// Controls REGISTER themselves with the ring instead of the page hand-
// maintaining a focusIndex. The ring keeps them in on-screen order (sorted by
// position each move, so it's robust to creation order, Repeaters, and
// conditional visibility) and owns Up/Down movement, Enter activation, and
// scroll-into-view. A control that forgets to register simply isn't in the
// ring — there's no index math to get subtly wrong, which is the failure mode
// that left buttons mouse-only on the old hand-rolled pages.
//
// A focusable control provides three things:
//   • an activate() method (run on Enter/Space, and from its own onClicked)
//   • a highlight bound to `<ring>.currentItem === <thisControl>`
//   • registration:  Component.onCompleted:   <ring>.register(<thisControl>)
//                    Component.onDestruction:  <ring>.unregister(<thisControl>)
//
// The host page forwards keys and sets the scroll context:
//   FocusRing { id: ring; scroller: theFlickable; originItem: theContentColumn }
//   Keys: Up/Down → ring.up()/ring.down() move between rows,
//         Left/Right → ring.left()/ring.right() move within a row,
//         Return/Enter/Space → ring.activate()
//
// Navigation is 2-D and row-aware: controls that sit on the same on-screen row
// (e.g. the three cards in a grid row) are grouped together, so Left/Right walk
// across a row and Up/Down jump to the row above/below (preserving the column
// by nearest horizontal centre). Single-control rows just move with Up/Down.
QtObject {
    id: ring

    // Registered focusable controls (unordered; grouped into rows on demand).
    property var items: []
    // The control currently focused, or null before the first navigation.
    property Item currentItem: null

    // Scroll-into-view context (optional). scroller is the Flickable that
    // scrolls; originItem is the coordinate space item positions are measured
    // in — normally the Flickable's content Column.
    property Flickable scroller: null
    property Item originItem: null

    // Controls on the same visual row are grouped when their top edges fall
    // within this many px (grid-row cards share a top edge; stacked controls
    // are separated by far more).
    property int rowTolerance: 16

    function register(item) {
        if (item && items.indexOf(item) === -1)
            items.push(item)
    }
    function unregister(item) {
        var i = items.indexOf(item)
        if (i !== -1) items.splice(i, 1)
        // Drop focus if the current control went away; the next move re-seeds.
        if (currentItem === item) currentItem = null
    }

    // Position of an item in originItem's coordinate space.
    function _pos(it) {
        var p = originItem ? it.mapToItem(originItem, 0, 0) : Qt.point(it.x, it.y)
        return { item: it, y: p.y, centerX: p.x + it.width / 2 }
    }

    // Visible controls grouped into rows (top-to-bottom), each row sorted
    // left-to-right.
    function _rows() {
        var ps = []
        for (var i = 0; i < items.length; i++)
            if (items[i] && items[i].visible) ps.push(_pos(items[i]))
        ps.sort(function(a, b) { return a.y - b.y })

        var rows = []
        var cur = []
        var rowY = null
        for (var j = 0; j < ps.length; j++) {
            if (rowY === null || Math.abs(ps[j].y - rowY) <= rowTolerance) {
                cur.push(ps[j])
                if (rowY === null) rowY = ps[j].y
            } else {
                rows.push(cur)
                cur = [ps[j]]
                rowY = ps[j].y
            }
        }
        if (cur.length) rows.push(cur)
        for (var r = 0; r < rows.length; r++)
            rows[r].sort(function(a, b) { return a.centerX - b.centerX })
        return rows
    }

    // {r, c} of the current control within rows, or null.
    function _locate(rows) {
        for (var r = 0; r < rows.length; r++)
            for (var c = 0; c < rows[r].length; c++)
                if (rows[r][c].item === currentItem) return { r: r, c: c }
        return null
    }

    function up()    { _moveRow(-1) }
    function down()  { _moveRow(1) }
    function left()  { _moveCol(-1) }
    function right() { _moveCol(1) }

    function _moveRow(delta) {
        var rows = _rows()
        if (rows.length === 0) { currentItem = null; return }
        var loc = _locate(rows)
        if (loc === null) {                       // first move seeds the top-left
            currentItem = rows[0][0].item; ensureVisible(currentItem); return
        }
        var nr = Math.max(0, Math.min(rows.length - 1, loc.r + delta))
        if (nr === loc.r) return
        // Preserve the column visually: pick the item in the new row whose
        // horizontal centre is nearest the current one.
        var curX = rows[loc.r][loc.c].centerX
        var best = rows[nr][0]
        var bestD = Math.abs(best.centerX - curX)
        for (var c = 1; c < rows[nr].length; c++) {
            var d = Math.abs(rows[nr][c].centerX - curX)
            if (d < bestD) { bestD = d; best = rows[nr][c] }
        }
        currentItem = best.item
        ensureVisible(currentItem)
    }

    function _moveCol(delta) {
        var rows = _rows()
        if (rows.length === 0) { currentItem = null; return }
        var loc = _locate(rows)
        if (loc === null) {
            currentItem = rows[0][0].item; ensureVisible(currentItem); return
        }
        var nc = Math.max(0, Math.min(rows[loc.r].length - 1, loc.c + delta))
        currentItem = rows[loc.r][nc].item
        ensureVisible(currentItem)
    }

    function activate() {
        if (currentItem && typeof currentItem.activate === "function")
            currentItem.activate()
    }

    function ensureVisible(item) {
        if (!scroller || !originItem || !item) return
        var p = item.mapToItem(originItem, 0, 0)
        var top = p.y
        var bottom = p.y + item.height
        var viewTop = scroller.contentY
        var viewBottom = scroller.contentY + scroller.height
        var maxY = Math.max(0, originItem.height - scroller.height)
        if (top < viewTop + 8)
            scroller.contentY = Math.max(0, top - 8)
        else if (bottom > viewBottom - 8)
            scroller.contentY = Math.min(maxY, bottom - scroller.height + 8)
    }
}
