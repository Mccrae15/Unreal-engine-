webpackHotUpdate("static\\development\\pages\\[view].js",{

/***/ "./src/DownloadManager/bifrost-download-manager.ts":
/*!*********************************************************!*\
  !*** ./src/DownloadManager/bifrost-download-manager.ts ***!
  \*********************************************************/
/*! exports provided: BifrostDownlaodManager */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "BifrostDownlaodManager", function() { return BifrostDownlaodManager; });
/* harmony import */ var _babel_runtime_helpers_esm_classCallCheck__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! @babel/runtime/helpers/esm/classCallCheck */ "./node_modules/@babel/runtime/helpers/esm/classCallCheck.js");
/* harmony import */ var _babel_runtime_helpers_esm_createClass__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! @babel/runtime/helpers/esm/createClass */ "./node_modules/@babel/runtime/helpers/esm/createClass.js");
/* harmony import */ var _babel_runtime_helpers_esm_defineProperty__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! @babel/runtime/helpers/esm/defineProperty */ "./node_modules/@babel/runtime/helpers/esm/defineProperty.js");




// Implementation
var BifrostDownlaodManager = /*#__PURE__*/function () {
  function BifrostDownlaodManager(socketManager) {
    Object(_babel_runtime_helpers_esm_classCallCheck__WEBPACK_IMPORTED_MODULE_0__["default"])(this, BifrostDownlaodManager);

    this.socketManager = socketManager;
  }

  Object(_babel_runtime_helpers_esm_createClass__WEBPACK_IMPORTED_MODULE_1__["default"])(BifrostDownlaodManager, [{
    key: "download",
    value: function download(payload) {
      this.socketManager.addToDownloadQueue(payload);
    }
  }, {
    key: "downloadAssets",
    value: function downloadAssets(payload) {
      this.socketManager.addItemsToDownloadQueue(payload);
    }
  }, {
    key: "downloadMetahuman",
    value: function downloadMetahuman(payload) {
      console.log("Received Metahuman payload: ", payload);
      this.socketManager.addMHToDownloadQueue(payload);
    }
  }, {
    key: "pause",
    value: function pause(id) {
      this.socketManager.pause(id);
    }
  }, {
    key: "resume",
    value: function resume(id) {
      this.socketManager.resume(id);
    }
  }, {
    key: "retry",
    value: function retry(id) {
      this.socketManager.retry(id);
    }
  }, {
    key: "cancel",
    value: function cancel(id) {
      this.socketManager.cancel(id);
    }
  }, {
    key: "removeFromQueue",
    value: function removeFromQueue(ids) {
      this.socketManager.removeFromQueue(ids);
    }
  }, {
    key: "cancelAll",
    value: function cancelAll() {
      this.socketManager.cancelAll();
    }
  }, {
    key: "openAssetInFinder",
    value: function openAssetInFinder(id) {
      this.socketManager.openAssetInFinder(id);
    }
  }, {
    key: "clearAll",
    value: function clearAll() {
      this.socketManager.clearAll();
    }
  }, {
    key: "removeMHFromQueue",
    value: function removeMHFromQueue(id) {
      this.socketManager.removeMHFromQueue(id);
    }
  }, {
    key: "export",
    value: function _export(items) {
      this.socketManager["export"](items);
    }
  }, {
    key: "progressivelyShowInScene",
    value: function progressivelyShowInScene(items) {
      this.socketManager.progressivelyShowInScene(items);
    }
  }, {
    key: "register",
    value: function register() {
      this.socketManager.setupSocket(8000);
    }
  }, {
    key: "unregister",
    value: function unregister() {
      alert("unregister");
      this.socketManager.closeSocket();
    }
  }, {
    key: "setLibraryPath",
    value: function setLibraryPath(path) {
      this.socketManager.setLibraryPath(path);
    }
  }, {
    key: "registerInAppNotificationCallback",
    value: function registerInAppNotificationCallback(onRecieve) {
      console.log("Register for in app notifications");
      this.socketManager.registerInAppNotificationCallback(onRecieve);
    }
  }, {
    key: "unregisterInAppNotificationCallback",
    value: function unregisterInAppNotificationCallback(onRecieve) {
      console.log("Unregister for in app notifications");
      this.socketManager.unregisterInAppNotificationCallback(onRecieve);
    }
  }], [{
    key: "getInstance",
    value: function getInstance(socketManager) {
      if (!this.instance) {
        this.instance = new BifrostDownlaodManager(socketManager);
      }

      return this.instance;
    }
  }]);

  return BifrostDownlaodManager;
}();

Object(_babel_runtime_helpers_esm_defineProperty__WEBPACK_IMPORTED_MODULE_2__["default"])(BifrostDownlaodManager, "instance", void 0);



/***/ })

})
//# sourceMappingURL=[view].js.da00e28dc212a717d0f9.hot-update.js.map