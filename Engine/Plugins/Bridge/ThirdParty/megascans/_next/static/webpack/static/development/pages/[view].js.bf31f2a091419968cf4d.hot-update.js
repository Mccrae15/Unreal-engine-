webpackHotUpdate("static\\development\\pages\\[view].js",{

/***/ "./src/Bifrost/useNetworkStore.ts":
/*!****************************************!*\
  !*** ./src/Bifrost/useNetworkStore.ts ***!
  \****************************************/
/*! exports provided: startPolling, stopPolling, default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "startPolling", function() { return startPolling; });
/* harmony export (binding) */ __webpack_require__.d(__webpack_exports__, "stopPolling", function() { return stopPolling; });
/* harmony import */ var zustand__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! zustand */ "./node_modules/zustand/index.js");

var useNetworkStore = Object(zustand__WEBPACK_IMPORTED_MODULE_0__["default"])(function () {
  return {
    connected: true
  };
});

var ping = function ping(url, timeout) {
  return new Promise(function (resolve) {
    var isOnline = function isOnline() {
      return resolve(true);
    };

    var isOffline = function isOffline() {
      return resolve(false);
    };

    var xhr = new XMLHttpRequest();
    xhr.onerror = isOffline;
    xhr.ontimeout = isOffline;

    xhr.onreadystatechange = function () {
      if (xhr.readyState === xhr.HEADERS_RECEIVED) {
        if (xhr.status) {
          isOnline();
        } else {
          isOffline();
        }
      }
    };

    xhr.open("HEAD", url);
    xhr.timeout = timeout;
    xhr.send();
  });
};

var pollingId;
var startPolling = function startPolling() {
  stopPolling();
  var interval = 10000;
  var url = "cdn.quixel.com/";
  var timeout = 7000;
  pollingId = setInterval(function () {
    ping(url, timeout).then(function (online) {
      if (online) {
        useNetworkStore.setState(function () {
          return {
            connected: true
          };
        });
      } else {
        useNetworkStore.setState(function () {
          return {
            connected: false
          };
        });
      }
    });
  }, interval);
  ping(url, timeout).then(function (online) {
    if (online) {
      useNetworkStore.setState(function () {
        return {
          connected: true
        };
      });
    } else {
      useNetworkStore.setState(function () {
        return {
          connected: false
        };
      });
    }
  });
};
var stopPolling = function stopPolling() {
  if (pollingId) {
    clearInterval(pollingId);
  }
};
/* harmony default export */ __webpack_exports__["default"] = (useNetworkStore);

/***/ })

})
//# sourceMappingURL=[view].js.bf31f2a091419968cf4d.hot-update.js.map