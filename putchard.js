// extern putchard(char);

"use strict";

if (typeof foreign !== "object") {
  var foreign = {};
}

var util = require("util");

foreign.putchard = function (char) {
  util.print(String.fromCharCode(char));
  return 0;
};
