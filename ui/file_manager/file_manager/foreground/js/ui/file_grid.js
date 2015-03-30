// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileGrid constructor.
 *
 * Represents grid for the Grid View in the File Manager.
 * @constructor
 * @extends {cr.ui.Grid}
 */

function FileGrid() {
  throw new Error('Use FileGrid.decorate');
}

/**
 * Inherits from cr.ui.Grid.
 */
FileGrid.prototype.__proto__ = cr.ui.Grid.prototype;

/**
 * Decorates an HTML element to be a FileGrid.
 * @param {!Element} self The grid to decorate.
 * @param {!MetadataModel} metadataModel File system metadata.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @param {!importer.HistoryLoader} historyLoader
 */
FileGrid.decorate = function(
    self, metadataModel, volumeManager, historyLoader) {
  cr.ui.Grid.decorate(self);
  self.__proto__ = FileGrid.prototype;
  self.metadataModel_ = metadataModel;
  self.volumeManager_ = volumeManager;
  self.historyLoader_ = historyLoader;

  /** @private {ListThumbnailLoader} */
  self.listThumbnailLoader_ = null;

  /** @private {number} */
  self.beginIndex_ = 0;

  /** @private {number} */
  self.endIndex_ = 0;

  /**
   * Reflects the visibility of import status in the UI.  Assumption: import
   * status is only enabled in import-eligible locations.  See
   * ImportController#onDirectoryChanged.  For this reason, the code in this
   * class checks if import status is visible, and if so, assumes that all the
   * files are in an import-eligible location.
   * TODO(kenobi): Clean this up once import status is queryable from metadata.
   *
   * @private {boolean}
   */
  self.importStatusVisible_ = true;

  /** @private {function(!Event)} */
  self.onThumbnailLoadedBound_ = self.onThumbnailLoaded_.bind(self);

  self.scrollBar_ = new ScrollBar();
  self.scrollBar_.initialize(self.parentElement, self);

  self.itemConstructor = function(entry) {
    var item = self.ownerDocument.createElement('li');
    FileGrid.Item.decorate(
        item,
        entry,
        /** @type {FileGrid} */ (self));
    return item;
  };

  self.relayoutRateLimiter_ =
      new AsyncUtil.RateLimiter(self.relayoutImmediately_.bind(self));

  var style = window.getComputedStyle(self);
  /** @private {number} */
  self.paddingLeft_ = parseFloat(style.paddingLeft);
  /** @private {number} */
  self.paddingTop_ = parseFloat(style.paddingTop);
};

/**
 * Sets list thumbnail loader.
 * @param {ListThumbnailLoader} listThumbnailLoader A list thumbnail loader.
 * @private
 */
FileGrid.prototype.setListThumbnailLoader = function(listThumbnailLoader) {
  if (this.listThumbnailLoader_) {
    this.listThumbnailLoader_.removeEventListener(
        'thumbnailLoaded', this.onThumbnailLoadedBound_);
  }

  this.listThumbnailLoader_ = listThumbnailLoader;

  if (this.listThumbnailLoader_) {
    this.listThumbnailLoader_.addEventListener(
        'thumbnailLoaded', this.onThumbnailLoadedBound_);
    this.listThumbnailLoader_.setHighPriorityRange(
        this.beginIndex_, this.endIndex_);
  }
};

/**
 * Handles thumbnail loaded event.
 * @param {!Event} event An event.
 * @private
 */
FileGrid.prototype.onThumbnailLoaded_ = function(event) {
  var listItem = this.getListItemByIndex(event.index);
  if (listItem) {
    var box = listItem.querySelector('.img-container');
    if (box) {
      FileGrid.setThumbnailImage_(
          assertInstanceof(box, HTMLDivElement),
          event.dataUrl,
          /* should animate */ true);
    }
  }
};

/**
 * @override
 */
FileGrid.prototype.mergeItems = function(beginIndex, endIndex) {
  cr.ui.List.prototype.mergeItems.call(this, beginIndex, endIndex);

  var afterFiller = this.afterFiller_;
  var columns = this.columns;

  for (var item = this.beforeFiller_.nextSibling; item != afterFiller;) {
    var next = item.nextSibling;
    if (isSpacer(item)) {
      // Spacer found on a place it mustn't be.
      this.removeChild(item);
      item = next;
      continue;
    }
    var index = item.listIndex;
    var nextIndex = index + 1;

    // Invisible pinned item could be outside of the
    // [beginIndex, endIndex). Ignore it.
    if (index >= beginIndex && nextIndex < endIndex &&
        (nextIndex < this.dataModel.getFolderCount()
            ? nextIndex % columns == 0
            : (nextIndex - this.dataModel.getFolderCount()) % columns == 0)) {
      if (isSpacer(next)) {
        // Leave the spacer on its place.
        item = next.nextSibling;
      } else {
        // Insert spacer.
        var spacer = this.ownerDocument.createElement('div');
        spacer.className = 'spacer';
        this.insertBefore(spacer, next);
        item = next;
      }
    } else
      item = next;
  }

  function isSpacer(child) {
    return child.classList.contains('spacer') &&
           child != afterFiller;  // Must not be removed.
  }

  // Make sure that grid item's selected attribute is updated just after the
  // mergeItems operation is done. This prevents shadow of selected grid items
  // from being animated unintentionally by redraw.
  for (var i = beginIndex; i < endIndex; i++) {
    var item = this.getListItemByIndex(i);
    if (!item)
      continue;
    var isSelected = this.selectionModel.getIndexSelected(i);
    if (item.selected != isSelected)
      item.selected = isSelected;
  }

  // Keep these values to set range when a new list thumbnail loader is set.
  this.beginIndex_ = beginIndex;
  this.endIndex_ = endIndex;
  if (this.listThumbnailLoader_ !== null)
    this.listThumbnailLoader_.setHighPriorityRange(beginIndex, endIndex);
};

/**
 * @override
 */
FileGrid.prototype.getItemTop = function(index) {
  if (index < this.dataModel.getFolderCount())
    return Math.floor(index / this.columns) * this.getFolderItemHeight_();

  var folderRows = Math.ceil(this.dataModel.getFolderCount() / this.columns);
  var indexInFiles = index - this.dataModel.getFolderCount();
  return folderRows * this.getFolderItemHeight_() +
      Math.floor(indexInFiles / this.columns) * this.getFileItemHeight_();
};

/**
 * @override
 */
FileGrid.prototype.getItemRow = function(index) {
  if (index < this.dataModel.getFolderCount())
    return Math.floor(index / this.columns);

  var folderRows = Math.ceil(this.dataModel.getFolderCount() / this.columns);
  var indexInFiles = index - this.dataModel.getFolderCount();
  return folderRows + Math.floor(indexInFiles / this.columns);
};

/**
 * Returns the column of an item which has given index.
 * @param {number} index The item index.
 */
FileGrid.prototype.getItemColumn = function(index) {
  if (index < this.dataModel.getFolderCount())
    return index % this.columns;

  var indexInFiles = index - this.dataModel.getFolderCount();
  return indexInFiles % this.columns;
};

/**
 * Return the item index which is placed at the given position.
 * If there is no item in the given position, returns the index of the last item
 * before the empty spaces.
 * @param {number} row The row index.
 * @param {number} column The column index.
 */
FileGrid.prototype.getItemIndex = function(row, column) {
  if (row < 0)
    return 0;
  var folderCount = this.dataModel.getFolderCount();
  var folderRows = Math.ceil(folderCount / this.columns);
  if (row < folderRows)
    return Math.min(row * this.columns + column, folderCount - 1);

  return Math.min(folderCount + (row - folderRows) * this.columns + column,
                  this.dataModel.length - 1);
};

/**
 * @override
 */
FileGrid.prototype.getFirstItemInRow = function(row) {
  var folderRows = Math.ceil(this.dataModel.getFolderCount() / this.columns);
  if (row < folderRows)
    return row * this.columns;

  return this.dataModel.getFolderCount() + (row - folderRows) * this.columns;
};

/**
 * @override
 */
FileGrid.prototype.scrollIndexIntoView = function(index) {
  var dataModel = this.dataModel;
  if (!dataModel || index < 0 || index >= dataModel.length)
    return false;

  var itemHeight = index < this.dataModel.getFolderCount() ?
      this.getFolderItemHeight_() : this.getFileItemHeight_();
  var scrollTop = this.scrollTop;
  var top = this.getItemTop(index);
  var clientHeight = this.clientHeight;

  var computedStyle = window.getComputedStyle(this);
  var paddingY = parseInt(computedStyle.paddingTop, 10) +
                 parseInt(computedStyle.paddingBottom, 10);
  var availableHeight = clientHeight - paddingY;

  var self = this;
  // Function to adjust the tops of viewport and row.
  var scrollToAdjustTop = function() {
      self.scrollTop = top;
      return true;
  };
  // Function to adjust the bottoms of viewport and row.
  var scrollToAdjustBottom = function() {
      self.scrollTop = top + itemHeight - availableHeight;
      return true;
  };

  // Check if the entire of given indexed row can be shown in the viewport.
  if (itemHeight <= availableHeight) {
    if (top < scrollTop)
      return scrollToAdjustTop();
    if (scrollTop + availableHeight < top + itemHeight)
      return scrollToAdjustBottom();
  } else {
    if (scrollTop < top)
      return scrollToAdjustTop();
    if (top + itemHeight < scrollTop + availableHeight)
      return scrollToAdjustBottom();
  }
  return false;
};

/**
 * @override
 */
FileGrid.prototype.getItemsInViewPort = function(scrollTop, clientHeight) {
  var beginRow = this.getRowForListOffset_(scrollTop);
  var endRow = this.getRowForListOffset_(scrollTop + clientHeight - 1) + 1;
  var beginIndex = this.getFirstItemInRow(beginRow);
  var endIndex = Math.min(this.getFirstItemInRow(endRow),
                          this.dataModel.length);
  var result = {
    first: beginIndex,
    length: endIndex - beginIndex,
    last: endIndex - 1
  };
  return result;
};

/**
 * @override
 */
FileGrid.prototype.getAfterFillerHeight = function(lastIndex) {
  var folderRows = Math.ceil(this.dataModel.getFolderCount() / this.columns);
  var fileRows =  Math.ceil(this.dataModel.getFileCount() / this.columns);
  var row = this.getItemRow(lastIndex - 1);
  if (row < folderRows) {
    return (folderRows - 1 - row) * this.getFolderItemHeight_() +
        fileRows * this.getFileItemHeight_();
  }
  var rowInFiles = row - folderRows;
  return (fileRows - 1 - rowInFiles) * this.getFileItemHeight_();
};

/**
 * Returns the height of folder items in grid view.
 * @return {number} The height of folder items.
 */
FileGrid.prototype.getFolderItemHeight_ = function() {
  return 48;  // TODO(fukino): Read from DOM and cache it.
};

/**
 * Returns the height of file items in grid view.
 * @return {number} The height of file items.
 */
FileGrid.prototype.getFileItemHeight_ = function() {
  return 188;  // TODO(fukino): Read from DOM and cache it.
};

/**
 * Returns index of a row which contains the given y-position(offset).
 * @param {number} offset The offset from the top of grid.
 * @return {number} Row index corresponding to the given offset.
 * @private
 */
FileGrid.prototype.getRowForListOffset_ = function(offset) {
  var folderRows = Math.ceil(this.dataModel.getFolderCount() / this.columns);
  if (offset < folderRows * this.getFolderItemHeight_())
    return Math.floor(offset / this.getFolderItemHeight_());

  var offsetInFiles = offset - folderRows * this.getFolderItemHeight_();
  return folderRows + Math.floor(offsetInFiles / this.getFileItemHeight_());
};

/**
 * @override
 */
FileGrid.prototype.createSelectionController = function(sm) {
  return new FileGridSelectionController(assert(sm), this);
};

/**
 * Updates items to reflect metadata changes.
 * @param {string} type Type of metadata changed.
 * @param {Array.<Entry>} entries Entries whose metadata changed.
 */
FileGrid.prototype.updateListItemsMetadata = function(type, entries) {
  var urls = util.entriesToURLs(entries);
  var boxes = this.querySelectorAll('.img-container');
  for (var i = 0; i < boxes.length; i++) {
    var box = boxes[i];
    var listItem = this.getListItemAncestor(box);
    var entry = listItem && this.dataModel.item(listItem.listIndex);
    if (!entry || urls.indexOf(entry.toURL()) === -1)
      continue;

    this.decorateThumbnailBox_(assertInstanceof(box, HTMLDivElement), entry);
    this.updateSharedStatus_(assert(listItem), entry);
  }
};

/**
 * Redraws the UI. Skips multiple consecutive calls.
 */
FileGrid.prototype.relayout = function() {
  this.relayoutRateLimiter_.run();
};

/**
 * Redraws the UI immediately.
 * @private
 */
FileGrid.prototype.relayoutImmediately_ = function() {
  this.startBatchUpdates();
  this.columns = 0;
  this.redraw();
  this.endBatchUpdates();
  cr.dispatchSimpleEvent(this, 'relayout');
};

/**
 * Decorates thumbnail.
 * @param {cr.ui.ListItem} li List item.
 * @param {!Entry} entry Entry to render a thumbnail for.
 * @private
 */
FileGrid.prototype.decorateThumbnail_ = function(li, entry) {
  li.className = 'thumbnail-item';
  if (entry)
    filelist.decorateListItem(li, entry, this.metadataModel_);

  var frame = li.ownerDocument.createElement('div');
  frame.className = 'thumbnail-frame';
  li.appendChild(frame);

  var box = li.ownerDocument.createElement('div');
  if (entry) {
    this.decorateThumbnailBox_(assertInstanceof(box, HTMLDivElement), entry);
  }
  frame.appendChild(box);

  var isDirectory = entry && entry.isDirectory;
  if (!isDirectory) {
    var active_checkmark = li.ownerDocument.createElement('div');
    active_checkmark.className = 'checkmark active';
    frame.appendChild(active_checkmark);
    var inactive_checkmark = li.ownerDocument.createElement('div');
    inactive_checkmark.className = 'checkmark inactive';
    frame.appendChild(inactive_checkmark);
  }

  var bottom = li.ownerDocument.createElement('div');
  bottom.className = 'thumbnail-bottom';
  var badge = li.ownerDocument.createElement('div');
  badge.className = 'badge';
  bottom.appendChild(badge);
  var detailIcon = filelist.renderFileTypeIcon(li.ownerDocument, entry);
  if (isDirectory) {
    var checkmark = li.ownerDocument.createElement('div');
    checkmark.className = 'detail-checkmark';
    detailIcon.appendChild(checkmark);
  }
  bottom.appendChild(detailIcon);
  bottom.appendChild(filelist.renderFileNameLabel(li.ownerDocument, entry));
  frame.appendChild(bottom);

  this.updateSharedStatus_(li, entry);
};

/**
 * Decorates the box containing a centered thumbnail image.
 *
 * @param {!HTMLDivElement} box Box to decorate.
 * @param {Entry} entry Entry which thumbnail is generating for.
 * @private
 */
FileGrid.prototype.decorateThumbnailBox_ = function(box, entry) {
  box.className = 'img-container';

  if (this.importStatusVisible_ &&
      importer.isEligibleType(entry)) {
    this.historyLoader_.getHistory().then(
        FileGrid.applyHistoryBadges_.bind(
            null,
            /** @type {!FileEntry} */ (entry),
            box));
  }

  if (entry.isDirectory) {
    box.setAttribute('generic-thumbnail', 'folder');
    return;
  }

  // Set thumbnail if it's already in cache.
  if (this.listThumbnailLoader_ &&
      this.listThumbnailLoader_.getThumbnailFromCache(entry)) {
    FileGrid.setThumbnailImage_(
        box,
        this.listThumbnailLoader_.getThumbnailFromCache(entry).dataUrl,
        /* should not animate */ false);
  } else {
    var mediaType = FileType.getMediaType(entry);
    box.setAttribute('generic-thumbnail', mediaType);
  }
};

/**
 * Added 'shared' class to icon and placeholder of a folder item.
 * @param {!HTMLLIElement} li The grid item.
 * @param {!Entry} entry File entry for the grid item.
 * @private
 */
FileGrid.prototype.updateSharedStatus_ = function(li, entry) {
  if (!entry.isDirectory)
    return;

  var shared = !!this.metadataModel_.getCache([entry], ['shared'])[0].shared;
  var box = li.querySelector('.img-container');
  if (box)
    box.classList.toggle('shared', shared);
  var icon = li.querySelector('.detail-icon');
  if (icon)
    icon.classList.toggle('shared', shared);
};

/**
 * Sets the visibility of the cloud import status column.
 * @param {boolean} visible
 */
FileGrid.prototype.setImportStatusVisible = function(visible) {
  this.importStatusVisible_ = visible;
};

/**
 * Sets thumbnail image to the box.
 * @param {!HTMLDivElement} box A div element to hold thumbnails.
 * @param {string} dataUrl Data url of thumbnail.
 * @param {boolean} shouldAnimate Whether the thumbanil is shown with animation
 *     or not.
 * @private
 */
FileGrid.setThumbnailImage_ = function(box, dataUrl, shouldAnimate) {
  var oldThumbnails = box.querySelectorAll('.thumbnail');

  var thumbnail = box.ownerDocument.createElement('div');
  thumbnail.classList.add('thumbnail');
  thumbnail.style.backgroundImage = 'url(' + dataUrl + ')';
  thumbnail.addEventListener('webkitAnimationEnd', function() {
    // Remove animation css once animation is completed in order not to animate
    // again when an item is attached to the dom again.
    thumbnail.classList.remove('animate');

    for (var i = 0; i < oldThumbnails.length; i++) {
      if (box.contains(oldThumbnails[i]))
        box.removeChild(oldThumbnails[i]);
    }
  });
  if (shouldAnimate)
    thumbnail.classList.add('animate');
  box.appendChild(thumbnail);
};

/**
 * Applies cloud import history badges as appropriate for the Entry.
 *
 * @param {!FileEntry} entry
 * @param {Element} box Box to decorate.
 * @param {!importer.ImportHistory} history
 *
 * @private
 */
FileGrid.applyHistoryBadges_ = function(entry, box, history) {
  history.wasImported(entry, importer.Destination.GOOGLE_DRIVE)
      .then(
          function(imported) {
            if (imported) {
              // TODO(smckay): update badges when history changes
              // "box" is currently the sibling of the elemement
              // we want to style. So rather than employing
              // a possibly-fragile sibling selector we just
              // plop the imported class on the parent of both.
              box.parentElement.classList.add('imported');
            } else {
              history.wasCopied(entry, importer.Destination.GOOGLE_DRIVE)
                  .then(
                      function(copied) {
                        if (copied) {
                          // TODO(smckay): update badges when history changes
                          // "box" is currently the sibling of the elemement
                          // we want to style. So rather than employing
                          // a possibly-fragile sibling selector we just
                          // plop the imported class on the parent of both.
                          box.parentElement.classList.add('copied');
                        }
                      });
            }
          });
};

/**
 * Item for the Grid View.
 * @constructor
 * @extends {cr.ui.ListItem}
 */
FileGrid.Item = function() {
  throw new Error();
};

/**
 * Inherits from cr.ui.ListItem.
 */
FileGrid.Item.prototype.__proto__ = cr.ui.ListItem.prototype;

Object.defineProperty(FileGrid.Item.prototype, 'label', {
  /**
   * @this {FileGrid.Item}
   * @return {string} Label of the item.
   */
  get: function() {
    return this.querySelector('filename-label').textContent;
  }
});

/**
 * @param {Element} li List item element.
 * @param {!Entry} entry File entry.
 * @param {FileGrid} grid Owner.
 */
FileGrid.Item.decorate = function(li, entry, grid) {
  li.__proto__ = FileGrid.Item.prototype;
  li = /** @type {!FileGrid.Item} */ (li);
  grid.decorateThumbnail_(li, entry);

  // Override the default role 'listitem' to 'option' to match the parent's
  // role (listbox).
  li.setAttribute('role', 'option');
};

/**
 * Obtains if the drag selection should be start or not by referring the mouse
 * event.
 * @param {MouseEvent} event Drag start event.
 * @return {boolean} True if the mouse is hit to the background of the list.
 */
FileGrid.prototype.shouldStartDragSelection = function(event) {
  var pos = DragSelector.getScrolledPosition(this, event);
  return this.getHitElements(pos.x, pos.y).length === 0;
};

/**
 * Obtains the column/row index that the coordinate points.
 * @param {number} coordinate Vertical/horizontal coordinate value that points
 *     column/row.
 * @param {number} step Length from a column/row to the next one.
 * @param {number} threshold Threshold that determines whether 1 offset is added
 *     to the return value or not. This is used in order to handle the margin of
 *     column/row.
 * @return {number} Index of hit column/row.
 * @private
 */
FileGrid.prototype.getHitIndex_ = function(coordinate, step, threshold) {
  var index = ~~(coordinate / step);
  return (coordinate % step >= threshold) ? index + 1 : index;
};

/**
 * Obtains the index list of elements that are hit by the point or the
 * rectangle.
 *
 * We should match its argument interface with FileList.getHitElements.
 *
 * @param {number} x X coordinate value.
 * @param {number} y Y coordinate value.
 * @param {number=} opt_width Width of the coordinate.
 * @param {number=} opt_height Height of the coordinate.
 * @return {Array.<number>} Index list of hit elements.
 */
FileGrid.prototype.getHitElements = function(x, y, opt_width, opt_height) {
  var currentSelection = [];
  var xInAvailableSpace = Math.max(0, x - this.paddingLeft_);
  var yInAvailableSpace = Math.max(0, y - this.paddingTop_);
  var right = xInAvailableSpace + (opt_width || 0);
  var bottom = yInAvailableSpace + (opt_height || 0);
  var itemMetrics = this.measureItem();
  var horizontalStartIndex = this.getHitIndex_(
      xInAvailableSpace, itemMetrics.width,
      itemMetrics.width - itemMetrics.marginRight);
  var horizontalEndIndex = Math.min(this.columns, this.getHitIndex_(
      right, itemMetrics.width, itemMetrics.marginLeft));
  var verticalStartIndex = this.getHitIndex_(
      yInAvailableSpace, itemMetrics.height,
      itemMetrics.height - itemMetrics.marginBottom);
  var verticalEndIndex = this.getHitIndex_(
      bottom, itemMetrics.height, itemMetrics.marginTop);

  for (var verticalIndex = verticalStartIndex;
       verticalIndex < verticalEndIndex;
       verticalIndex++) {
    var indexBase = this.getFirstItemInRow(verticalIndex);
    for (var horizontalIndex = horizontalStartIndex;
         horizontalIndex < horizontalEndIndex;
         horizontalIndex++) {
      var index = indexBase + horizontalIndex;
      if (0 <= index && index < this.dataModel.length)
        currentSelection.push(index);
    }
  }
  return currentSelection;
};

/**
 * Selection controller for the file grid.
 * @param {!cr.ui.ListSelectionModel} selectionModel The selection model to
 *     interact with.
 * @param {!cr.ui.Grid} grid The grid to interact with.
 * @constructor
 * @extends {cr.ui.GridSelectionController}
 * @struct
 * @suppress {checkStructDictInheritance}
 */
function FileGridSelectionController(selectionModel, grid) {
  cr.ui.GridSelectionController.call(this, selectionModel, grid);
}

FileGridSelectionController.prototype = /** @struct */ {
  __proto__: cr.ui.GridSelectionController.prototype
};

/** @override */
FileGridSelectionController.prototype.handlePointerDownUp = function(e, index) {
  filelist.handlePointerDownUp.call(this, e, index);
};

/** @override */
FileGridSelectionController.prototype.handleKeyDown = function(e) {
  filelist.handleKeyDown.call(this, e);
};

/** @override */
FileGridSelectionController.prototype.getIndexBelow = function(index) {
  if (this.isAccessibilityEnabled())
    return this.getIndexAfter(index);
  if (index === this.getLastIndex())
    return -1;

  var row = this.grid_.getItemRow(index);
  var col = this.grid_.getItemColumn(index);
  return this.grid_.getItemIndex(row + 1, col);
};

/** @override */
FileGridSelectionController.prototype.getIndexAbove = function(index) {
  if (this.isAccessibilityEnabled())
    return this.getIndexBefore(index);
  if (index == 0)
    return -1;

  var row = this.grid_.getItemRow(index);
  var col = this.grid_.getItemColumn(index);
  return this.grid_.getItemIndex(row - 1, col);
};
