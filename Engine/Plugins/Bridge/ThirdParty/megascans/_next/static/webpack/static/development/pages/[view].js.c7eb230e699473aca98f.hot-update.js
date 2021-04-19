webpackHotUpdate("static\\development\\pages\\[view].js",{

/***/ "./src/AssetsGrid/useAssetsList.ts":
/*!*****************************************!*\
  !*** ./src/AssetsGrid/useAssetsList.ts ***!
  \*****************************************/
/*! exports provided: default */
/***/ (function(module, __webpack_exports__, __webpack_require__) {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _babel_runtime_helpers_esm_toConsumableArray__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! @babel/runtime/helpers/esm/toConsumableArray */ "./node_modules/@babel/runtime/helpers/esm/toConsumableArray.js");
/* harmony import */ var _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! @babel/runtime/regenerator */ "./node_modules/@babel/runtime/regenerator/index.js");
/* harmony import */ var _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_1___default = /*#__PURE__*/__webpack_require__.n(_babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_1__);
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! react */ "./node_modules/react/index.js");
/* harmony import */ var react__WEBPACK_IMPORTED_MODULE_2___default = /*#__PURE__*/__webpack_require__.n(react__WEBPACK_IMPORTED_MODULE_2__);
/* harmony import */ var _types__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../types */ "./src/types.ts");
/* harmony import */ var _algoliaAssetsFetcher__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../algoliaAssetsFetcher */ "./src/algoliaAssetsFetcher.ts");
/* harmony import */ var _mhcAssetsFetcher__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ../mhcAssetsFetcher */ "./src/mhcAssetsFetcher.ts");
/* harmony import */ var _Auth_useAuthSWR__WEBPACK_IMPORTED_MODULE_6__ = __webpack_require__(/*! ../Auth/useAuthSWR */ "./src/Auth/useAuthSWR.ts");
/* harmony import */ var _useAssetsBrowseParams__WEBPACK_IMPORTED_MODULE_7__ = __webpack_require__(/*! ./useAssetsBrowseParams */ "./src/AssetsGrid/useAssetsBrowseParams.ts");
/* harmony import */ var _assertUnreachable__WEBPACK_IMPORTED_MODULE_8__ = __webpack_require__(/*! ../assertUnreachable */ "./src/assertUnreachable.ts");
/* harmony import */ var _LocalLibraryManager__WEBPACK_IMPORTED_MODULE_9__ = __webpack_require__(/*! ../LocalLibraryManager */ "./src/LocalLibraryManager/index.ts");
/* harmony import */ var _useAssetsCount__WEBPACK_IMPORTED_MODULE_10__ = __webpack_require__(/*! ./useAssetsCount */ "./src/AssetsGrid/useAssetsCount.ts");
/* harmony import */ var react_redux__WEBPACK_IMPORTED_MODULE_11__ = __webpack_require__(/*! react-redux */ "./node_modules/react-redux/es/index.js");
/* harmony import */ var src_User_actions__WEBPACK_IMPORTED_MODULE_12__ = __webpack_require__(/*! src/User/actions */ "./src/User/actions.js");














function getFetcher(assetBrowseParams) {
  switch (assetBrowseParams.view) {
    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].metahumans:
      return _mhcAssetsFetcher__WEBPACK_IMPORTED_MODULE_5__["default"];

    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].home:
    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].collections:
    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].favorites:
    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].free:
    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].purchased:
      return _algoliaAssetsFetcher__WEBPACK_IMPORTED_MODULE_4__["default"];
    // Don't need to handle the Locals case

    case _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].local:
      return function _callee() {
        return _babel_runtime_regenerator__WEBPACK_IMPORTED_MODULE_1___default.a.async(function _callee$(_context) {
          while (1) {
            switch (_context.prev = _context.next) {
              case 0:
                return _context.abrupt("return", {
                  assets: [],
                  totalPages: 0
                });

              case 1:
              case "end":
                return _context.stop();
            }
          }
        }, null, null, null, Promise);
      };

    default:
      Object(_assertUnreachable__WEBPACK_IMPORTED_MODULE_8__["default"])(assetBrowseParams.view);
  }
}

function localsTabSelected(assetBrowseParams) {
  return !!assetBrowseParams && assetBrowseParams.view === _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].local;
}

function useAssetsList(page) {
  var _assetsBrowseParams, _assetsBrowseParams2;

  var assetsPerPage = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 50;
  var config = arguments.length > 2 ? arguments[2] : undefined;
  var assetsBrowseParams = Object(_useAssetsBrowseParams__WEBPACK_IMPORTED_MODULE_7__["default"])();
  var dispatch = Object(react_redux__WEBPACK_IMPORTED_MODULE_11__["useDispatch"])();

  if (config === null || config === void 0 ? void 0 : config.overrideBrowseParams) {
    assetsBrowseParams = config.assetsBrowseParams;
  } // For Locals Tab - we don't want to use SWR
  // We want to get assets directly from the bifrost store
  // And we want to reset our local library on tab switch


  Object(react__WEBPACK_IMPORTED_MODULE_2__["useEffect"])(function () {
    if (assetsBrowseParams && localsTabSelected(assetsBrowseParams)) {
      Object(_LocalLibraryManager__WEBPACK_IMPORTED_MODULE_9__["getLocalLibraryManger"])().fetchLocalAssets(assetsBrowseParams);
    }
  }, [(_assetsBrowseParams = assetsBrowseParams) === null || _assetsBrowseParams === void 0 ? void 0 : _assetsBrowseParams.view]);

  var _useState = Object(react__WEBPACK_IMPORTED_MODULE_2__["useState"])(undefined),
      response = _useState[0],
      setResponse = _useState[1];

  var assetsBrowseKey = assetsBrowseParams && JSON.stringify(assetsBrowseParams);
  var fetcher = assetsBrowseParams ? getFetcher(assetsBrowseParams) : undefined;

  if (((_assetsBrowseParams2 = assetsBrowseParams) === null || _assetsBrowseParams2 === void 0 ? void 0 : _assetsBrowseParams2.view) === _types__WEBPACK_IMPORTED_MODULE_3__["PageView"].metahumans) {
    dispatch(Object(src_User_actions__WEBPACK_IMPORTED_MODULE_12__["loadDHIAssets"])({}));
  }

  var _useAuthSWR = Object(_Auth_useAuthSWR__WEBPACK_IMPORTED_MODULE_6__["default"])(false, assetsBrowseKey ? [assetsBrowseKey, page, assetsPerPage] : null, fetcher),
      data = _useAuthSWR.data,
      error = _useAuthSWR.error;

  var localAssetsResponse = assetsBrowseParams && Object(_LocalLibraryManager__WEBPACK_IMPORTED_MODULE_9__["getLocalLibraryManger"])().loadLocalAssets(assetsBrowseParams, page, assetsPerPage);
  var loading = assetsBrowseKey && !data && !error;
  Object(react__WEBPACK_IMPORTED_MODULE_2__["useEffect"])(function () {
    if (data) {
      var assets = data.assets,
          totalPages = data.totalPages;
      var newAssets = page > 0 && (response === null || response === void 0 ? void 0 : response.assets) ? [].concat(Object(_babel_runtime_helpers_esm_toConsumableArray__WEBPACK_IMPORTED_MODULE_0__["default"])(response.assets), Object(_babel_runtime_helpers_esm_toConsumableArray__WEBPACK_IMPORTED_MODULE_0__["default"])(assets)) : assets;
      _useAssetsCount__WEBPACK_IMPORTED_MODULE_10__["default"].getState().setCount(newAssets.length);
      setResponse({
        page: page,
        totalPages: totalPages,
        assets: newAssets
      });
    }

    if (assetsBrowseParams && localsTabSelected(assetsBrowseParams)) {
      var _newAssets = page > 0 && (localAssetsResponse === null || localAssetsResponse === void 0 ? void 0 : localAssetsResponse.assets) && (response === null || response === void 0 ? void 0 : response.assets) ? [].concat(Object(_babel_runtime_helpers_esm_toConsumableArray__WEBPACK_IMPORTED_MODULE_0__["default"])(response === null || response === void 0 ? void 0 : response.assets), Object(_babel_runtime_helpers_esm_toConsumableArray__WEBPACK_IMPORTED_MODULE_0__["default"])(localAssetsResponse.assets)) : (localAssetsResponse === null || localAssetsResponse === void 0 ? void 0 : localAssetsResponse.assets) || []; // remove duplicates


      _newAssets = _newAssets.filter(function (asset, index, self) {
        return self.findIndex(function (item) {
          return item.id === asset.id;
        }) === index;
      });
      _useAssetsCount__WEBPACK_IMPORTED_MODULE_10__["default"].getState().setCount(_newAssets.length);
      setResponse({
        page: page,
        totalPages: (localAssetsResponse === null || localAssetsResponse === void 0 ? void 0 : localAssetsResponse.totalPages) || 0,
        assets: _newAssets
      });
    }
  }, [loading, assetsBrowseKey, page]);
  return {
    assets: response === null || response === void 0 ? void 0 : response.assets,
    page: response === null || response === void 0 ? void 0 : response.page,
    totalPages: response === null || response === void 0 ? void 0 : response.totalPages,
    loading: loading
  };
}

/* harmony default export */ __webpack_exports__["default"] = (useAssetsList);

/***/ })

})
//# sourceMappingURL=[view].js.c7eb230e699473aca98f.hot-update.js.map