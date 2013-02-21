// extern sin(arg);
// extern cos(arg);
// extern atan2(arg1 arg2);
//
// atan2(sin(.4), cos(42))

function Teleidoscope(stdlib, foreign, heap) {
  "use asm";

  var sin = stdlib.Math.sin;
  var cos = stdlib.Math.cos;
  var atan2 = stdlib.Math.atan2;

  function $main() {
    return atan2(sin(.4), cos(42.));
  }

  return { main: $main };
}

var v = Teleidoscope({ Infinity: Infinity, NaN: NaN, Math: Math }).main();
if (v !== undefined) {
  console.log(v);
}
