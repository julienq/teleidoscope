// extern putchard(char)
// def printstar(n)
//   for i = 1, i < n, 1.0 in
//     putchard(42);  # ascii 42 = '*'
//
// # print 72 '*' characters
// printstar(72);
// # followed by a newline
// putchard(10);

function Teleidoscope(stdlib, foreign, heap) {
  "use asm";

  var putchard = foreign.putchard;

  function printstar(n) {
    n = +n;
    var i = 1.;
    for (; (n - i)|0; i = i + 1.) putchard(42.);
    return;
  }

  function $main() {
    printstar(72.);
    putchard(10.);
    return;
  }

  return { main: $main };
}

var foreign = {
  putchard: function (n) {
    require("util").print(String.fromCharCode(n));
  }
};

var v = Teleidoscope({ Infinity: Infinity, NaN: NaN, Math: Math }, foreign)
  .main();
if (v !== undefined) {
  console.log(v);
}
