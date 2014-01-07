<?php
/**
 * @file
 * Displays the newspaper page controls.
 */
?>
<div class="islandora-openseadragon-controls clearfix" id="islandora-openseadragon-controls">
  <div class="islandora-newspaper-controls-toolbar clearfix" id="islandora-newspaper-controls-toolbar">
    <?php print theme('item_list', array('items' => $controls, 'attributes' => array('class' => array('items', 'inline')))); ?>
  </div>
</div>