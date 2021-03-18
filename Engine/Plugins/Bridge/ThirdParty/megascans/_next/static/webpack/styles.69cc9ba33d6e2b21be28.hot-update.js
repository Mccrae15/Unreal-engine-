webpackHotUpdate("styles",{

/***/ "./src/AssetsGrid/components/AssetGridItem.module.scss":
/*!*************************************************************!*\
  !*** ./src/AssetsGrid/components/AssetGridItem.module.scss ***!
  \*************************************************************/
/*! no static exports found */
/***/ (function(module, exports, __webpack_require__) {

// extracted by mini-css-extract-plugin
module.exports = {"wrap":"wrap___1lfmB","favorited":"favorited___2p8f0","badge":"badge___s2VJ7","textLabel":"textLabel___HrKD6","contextMenu":"contextMenu___3qW8g"};;
    if (true) {
      var injectCss = function injectCss(prev, href) {
        var link = prev.cloneNode();
        link.href = href;
        link.onload = function() {
          prev.parentNode.removeChild(prev);
        };
        prev.stale = true;
        prev.parentNode.insertBefore(link, prev);
      };
      module.hot.dispose(function() {
        window.__webpack_reload_css__ = true;
      });
      if (window.__webpack_reload_css__) {
        module.hot.__webpack_reload_css__ = false;
        console.log("[HMR] Reloading stylesheets...");
        var prefix = document.location.protocol + '//' + document.location.host;
        document
          .querySelectorAll("link[href][rel=stylesheet]")
          .forEach(function(link) {
            if (!link.href.match(prefix) || link.stale) return;
            injectCss(link, link.href.split("?")[0] + "?unix=1616075438530");
          });
      }
    }
  

/***/ })

})
//# sourceMappingURL=styles.69cc9ba33d6e2b21be28.hot-update.js.map