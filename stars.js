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
    var i = 0.;
    for (i = 1.; i < n; i = i + 1.) putchard(42.);
    return 0.;
  }

  function $main() {
    printstar(72.);
    return +putchard(10.);
  }

  return { main: $main };
}

var foreign = {
  putchard: function (n) {
    require("util").print(String.fromCharCode(n));
    return 0;
  }
};

console.log(
  Teleidoscope({ Infinity: Infinity, NaN: NaN, Math: Math }, foreign).main()
);
