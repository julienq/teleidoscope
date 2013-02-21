// def fib(x)
//   if x < 3 then
//     1
//   else
//     fib(x - 1) + fib(x - 2)
//
// # Compute the 40th number
// fib(40)

function Teleidoscope(stdlib, foreign, heap) {
  "use asm";

  function fib(x) {
    x = +x;
    if (x < 3.) return 1.; else return +fib(x - 1.) + +fib(x - 2.);
  }

  function $main() {
    return +fib(40.);
  }

  return { main: $main };
}

var v = Teleidoscope({ Infinity: Infinity, NaN: NaN, Math: Math }).main();
if (v !== undefined) {
  console.log(v);
}
