/**
 * @file
 * Islandora Newspaper scripts file.
 */

(function ($) {
  // Select page
  Drupal.behaviors.islandoraNewspaperSelectPage = {
    attach: function(context, settings) {
      if (!$(".page-select").hasClass('processed')) {
        $(".page-select").change(function(e) {
          var pid = $("option:selected", this).attr('value');
          if(pid != '#Download') {
            window.location = Drupal.settings.basePath + 'islandora/object/' + pid; // check plain?
           }
        });
        $(".page-select").addClass('processed');
      }
    }
  };
})(jQuery);

(function ($) {
    // Select page
    Drupal.behaviors.islandoraNewspaperScrollTop = {
        attach: function(context, settings) {
            $('.vertical-tab-button a').click(function() {
                $('html, body').animate({
                    scrollTop: $("#content").offset().top
                }, 45);
            })
        }
    };
})(jQuery);

