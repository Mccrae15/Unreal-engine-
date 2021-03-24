webpackHotUpdate("static\\development\\pages\\[view].js",{

/***/ "./src/Assets/useAssetDownload.ts":
/*!****************************************!*\
  !*** ./src/Assets/useAssetDownload.ts ***!
  \****************************************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _babel_runtime_helpers_esm_defineProperty__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! @babel/runtime/helpers/esm/defineProperty */ "./node_modules/@babel/runtime/helpers/esm/defineProperty.js");
/* harmony import */ var _babel_runtime_helpers_esm_slicedToArray__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! @babel/runtime/helpers/esm/slicedToArray */ "./node_modules/@babel/runtime/helpers/esm/slicedToArray.js");
/* harmony import */ var _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! @babel/runtime/regenerator */ "./node_modules/@babel/runtime/regenerator/index.js");
/* harmony import */ var _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(_babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! react-redux */ "./node_modules/react-redux/es/index.js");
/* harmony import */ var _DownloadManager__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../DownloadManager */ "./src/DownloadManager/index.ts");
/* harmony import */ var _User_actions__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ../User/actions */ "./src/User/actions.js");
/* harmony import */ var PlatformImports__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! PlatformImports */ "./platform-imports.ts");
/* harmony import */ var _Common_utils__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ../Common/utils */ "./src/Common/utils/index.js");
/* harmony import */ var _useUser__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! ../useUser */ "./src/useUser.ts");
/* harmony import */ var _AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(/*! ../AssetRightPanel/useTierForm */ "./src/AssetRightPanel/useTierForm.ts");
/* harmony import */ var _Bifrost_useDHIStore__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(/*! ../Bifrost/useDHIStore */ "./src/Bifrost/useDHIStore.ts");
/* harmony import */ var src_AssetsGrid_useMultiSelect__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(/*! src/AssetsGrid/useMultiSelect */ "./src/AssetsGrid/useMultiSelect.ts");
/* harmony import */ var src_Bifrost_useBifrostStore__WEBPACK_IMPORTED_MODULE_12__ = __webpack_require__(/*! src/Bifrost/useBifrostStore */ "./src/Bifrost/useBifrostStore.ts");
/* harmony import */ var _appConfig__WEBPACK_IMPORTED_MODULE_13__ = __webpack_require__(/*! ../appConfig */ "./src/appConfig.ts");
/* harmony import */ var src_Bifrost_useNetworkStore__WEBPACK_IMPORTED_MODULE_14__ = __webpack_require__(/*! src/Bifrost/useNetworkStore */ "./src/Bifrost/useNetworkStore.ts");




function ownKeys(object, enumerableOnly) { var keys = Object.keys(object); if (Object.getOwnPropertySymbols) { var symbols = Object.getOwnPropertySymbols(object); if (enumerableOnly) symbols = symbols.filter(function (sym) { return Object.getOwnPropertyDescriptor(object, sym).enumerable; }); keys.push.apply(keys, symbols); } return keys; }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; if (i % 2) { ownKeys(Object(source), true).forEach(function (key) { Object(_babel_runtime_helpers_esm_defineProperty__WEBPACK_IMPORTED_MODULE_0__["default"])(target, key, source[key]); }); } else if (Object.getOwnPropertyDescriptors) { Object.defineProperties(target, Object.getOwnPropertyDescriptors(source)); } else { ownKeys(Object(source)).forEach(function (key) { Object.defineProperty(target, key, Object.getOwnPropertyDescriptor(source, key)); }); } } return target; }














function useAssetDownload(assetOrAssets) {
  var _useUser = Object(_useUser__WEBPACK_IMPORTED_MODULE_8__["default"])(),
      loggedIn = _useUser.user.loggedIn,
      subscription = _useUser.subscription;

  var dispatch = Object(react_redux__WEBPACK_IMPORTED_MODULE_3__["useDispatch"])();
  var tier = Object(_AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__["default"])(function (state) {
    return state.fields.tier.value;
  });

  var downloadAsset = function downloadAsset(asset) {
    var _thumb;

    var progressive = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : false;
    var id = asset.id,
        name = asset.name,
        type = asset.type;

    if (isAssetDownloaded(asset)) {
      return;
    }

    var thumb = ((_thumb = asset.thumb) === null || _thumb === void 0 ? void 0 : _thumb.url) || asset.thumb.url;
    var uAssetPreviews = asset.uAssetPreviews || undefined;

    if (type === "metahuman") {
      dispatch(Object(_User_actions__WEBPACK_IMPORTED_MODULE_5__["downloadMHAsset"])(id));
    } else {
      Object(_DownloadManager__WEBPACK_IMPORTED_MODULE_4__["getDownloadManager"])().download({
        id: id,
        name: name,
        assetType: type,
        thumb: thumb,
        tier: _AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__["default"].getState().fields.tier.value,
        progressive: progressive,
        uAssetPreviews: uAssetPreviews
      });
    }
  };

  var isAssetDownloaded = function isAssetDownloaded(asset) {
    var downloadedAssets = src_Bifrost_useBifrostStore__WEBPACK_IMPORTED_MODULE_12__["default"].getState().localLibrary.assets;
    var tier = _AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__["default"].getState().fields.tier.value;
    return downloadedAssets.findIndex(function (downloadedAsset) {
      return downloadedAsset.id === asset.id && downloadedAsset.tiers.includes(tier);
    }) !== -1;
  };

  var getAssets = function getAssets(progressive) {
    var _useMultiSelect$getSt = src_AssetsGrid_useMultiSelect__WEBPACK_IMPORTED_MODULE_11__["default"].getState(),
        assets = _useMultiSelect$getSt.assets;

    var _useNetworkStore$getS = src_Bifrost_useNetworkStore__WEBPACK_IMPORTED_MODULE_14__["default"].getState(),
        connected = _useNetworkStore$getS.connected;

    var alreadyDownloadedAssets = [];
    var assetsToDownload = [];
    var tier = _AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__["default"].getState().fields.tier.value;

    if (Object.keys(assets).length > 1) {
      Object.keys(assets).map(function (id) {
        var asset = assets[id];
        return {
          id: id,
          name: asset.name,
          assetType: asset.type,
          thumb: asset.thumb.url,
          tier: tier,
          progressive: progressive,
          uAssetPreviews: asset.uAssetPreviews
        };
      }).forEach(function (asset) {
        if (isAssetDownloaded(asset)) {
          alreadyDownloadedAssets.push(asset);
        } else {
          assetsToDownload.push(asset);
        }
      });
    } else {
      var _thumb2;

      if (typeof assetOrAssets === "undefined") {
        return [[], []];
      }

      var asset = assetOrAssets;
      var id = asset.id,
          name = asset.name,
          type = asset.type;
      var thumb = ((_thumb2 = asset.thumb) === null || _thumb2 === void 0 ? void 0 : _thumb2.url) || asset.thumb.url;
      var uAssetPreviews = asset.uAssetPreviews || undefined;

      if (isAssetDownloaded(asset)) {
        alreadyDownloadedAssets.push({
          id: id,
          name: name,
          assetType: type,
          thumb: thumb,
          tier: tier,
          progressive: progressive,
          uAssetPreviews: uAssetPreviews
        });
      } else if (connected) {
        assetsToDownload.push({
          id: id,
          name: name,
          assetType: type,
          thumb: thumb,
          tier: tier,
          progressive: progressive,
          uAssetPreviews: uAssetPreviews
        });
      }
    }

    return [alreadyDownloadedAssets, assetsToDownload];
  };

  var fetchPreviews = function fetchPreviews(asset) {
    var tiersToFetch, fullResRawResponse, urls;
    return _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2___default.a.async(function fetchPreviews$(_context) {
      while (1) {
        switch (_context.prev = _context.next) {
          case 0:
            tiersToFetch = [4];
            _context.next = 3;
            return _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2___default.a.awrap(PlatformImports__WEBPACK_IMPORTED_MODULE_6__["axios"].post("".concat(_appConfig__WEBPACK_IMPORTED_MODULE_13__["default"].uassets.baseUrl, "/v1/downloads/uasset"), {
              asset: asset.id,
              tiers: tiersToFetch
            }, {
              headers: {
                Accept: "application/json",
                "Content-Type": "application/json",
                Authorization: "Bearer ".concat(Object(_Common_utils__WEBPACK_IMPORTED_MODULE_7__["getAuthTokenFromCookies"])().token)
              }
            }));

          case 3:
            fullResRawResponse = _context.sent;
            urls = fullResRawResponse.data;
            return _context.abrupt("return", urls.uasset);

          case 6:
          case "end":
            return _context.stop();
        }
      }
    }, null, null, null, Promise);
  };

  var startProgressiveDownloads = function startProgressiveDownloads() {
    var _getAssets = getAssets(true),
        _getAssets2 = Object(_babel_runtime_helpers_esm_slicedToArray__WEBPACK_IMPORTED_MODULE_1__["default"])(_getAssets, 2),
        _ = _getAssets2[0],
        assetsToDownload = _getAssets2[1];

    console.log(assetsToDownload); // Have to get UAssetPreviews for assets that don't have those

    if (assetsToDownload.length > 0) {
      if (assetsToDownload[0].uAssetPreviews) {
        // Already have UAssetPreviews (Non-Local Assets)
        Object(_DownloadManager__WEBPACK_IMPORTED_MODULE_4__["getDownloadManager"])().downloadAssets(assetsToDownload);
      } else {
        // Don't have UAssetPreviews (Local Assets)
        assetsToDownload.forEach(function _callee(asset) {
          var previews;
          return _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2___default.a.async(function _callee$(_context2) {
            while (1) {
              switch (_context2.prev = _context2.next) {
                case 0:
                  _context2.next = 2;
                  return _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_2___default.a.awrap(fetchPreviews(asset));

                case 2:
                  previews = _context2.sent;
                  asset.uAssetPreviews = previews;
                  Object(_DownloadManager__WEBPACK_IMPORTED_MODULE_4__["getDownloadManager"])().downloadAssets([asset]);

                case 5:
                case "end":
                  return _context2.stop();
              }
            }
          }, null, null, null, Promise);
        });
      }
    }
  };

  var downloadAssets = function downloadAssets() {
    if (typeof assetOrAssets === "undefined") {
      return;
    }

    if (!Array.isArray(assetOrAssets)) {
      return downloadAsset(assetOrAssets);
    }

    var assets = assetOrAssets;
    var dhiAssets = assets.filter(function (asset) {
      return asset.type === "metahuman" && !isAssetDownloaded(asset);
    });
    var msAssets = assets.filter(function (asset) {
      return asset.type !== "metahuman" && !isAssetDownloaded(asset);
    });

    var wrappedInterval = function wrappedInterval(assets) {
      var timeInterval = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
      assets.forEach(function (asset, index) {
        setTimeout(function () {
          downloadAsset(asset);
        }, index * timeInterval);
      });
    };

    var dhiTimeInterval = 1500;
    wrappedInterval(dhiAssets, dhiTimeInterval);

    if (msAssets.length > 1) {
      var selectedAssets = msAssets.map(function (msAsset) {
        return {
          id: msAsset.id,
          name: msAsset.name,
          assetType: msAsset.type,
          thumb: msAsset.thumb.url,
          tier: _AssetRightPanel_useTierForm__WEBPACK_IMPORTED_MODULE_9__["default"].getState().fields.tier.value,
          progressive: false
        };
      });
      Object(_DownloadManager__WEBPACK_IMPORTED_MODULE_4__["getDownloadManager"])().downloadAssets(selectedAssets);
    } else if (msAssets.length === 1) {
      wrappedInterval(msAssets, 0);
    }
  };

  var cancelDownload = function cancelDownload(assetId, generating) {
    if (generating) {
      cancelGeneration(assetId);
    } else {
      Object(_DownloadManager__WEBPACK_IMPORTED_MODULE_4__["getDownloadManager"])().cancel(assetId);
    }
  };

  var cancelGeneration = function cancelGeneration(assetId) {
    var _useDHIStore$getState = _Bifrost_useDHIStore__WEBPACK_IMPORTED_MODULE_10__["default"].getState(),
        downloadingDHIAssets = _useDHIStore$getState.downloadingDHIAssets;

    dispatch(Object(_User_actions__WEBPACK_IMPORTED_MODULE_5__["cancelDHIGeneration"])(assetId));
    _Bifrost_useDHIStore__WEBPACK_IMPORTED_MODULE_10__["default"].setState({
      downloadingDHIAssets: _objectSpread({}, downloadingDHIAssets, Object(_babel_runtime_helpers_esm_defineProperty__WEBPACK_IMPORTED_MODULE_0__["default"])({}, assetId, _objectSpread({}, downloadingDHIAssets[assetId], {
        state: "canceled"
      })))
    });
  };

  var canDownload = function canDownload(asset) {
    if ((asset === null || asset === void 0 ? void 0 : asset.type) === "metahuman") {
      return {
        downloadAssets: downloadAssets,
        cancelDownload: cancelDownload,
        tier: tier
      };
    } else if ((subscription === null || subscription === void 0 ? void 0 : subscription.planId) === "unreal-unlimited") {
      return {
        downloadAssets: downloadAssets,
        cancelDownload: cancelDownload,
        tier: tier
      };
    }
  };

  if (loggedIn) {
    if (Array.isArray(assetOrAssets)) {
      if (assetOrAssets.every(canDownload)) {
        return {
          downloadAssets: downloadAssets,
          startProgressiveDownloads: startProgressiveDownloads,
          cancelDownload: cancelDownload,
          tier: tier
        };
      }
    } else {
      if (assetOrAssets && canDownload(assetOrAssets)) {
        return {
          downloadAssets: downloadAssets,
          startProgressiveDownloads: startProgressiveDownloads,
          cancelDownload: cancelDownload,
          tier: tier
        };
      }
    }
  }

  return {
    tier: tier
  };
}

/* harmony default export */ __webpack_exports__["default"] = (useAssetDownload);

/***/ })

})
//# sourceMappingURL=[view].js.55b86578fb3b4a62a88b.hot-update.js.map