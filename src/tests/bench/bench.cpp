#include "vmath.h"
#include "benchmark/benchmark.h"

using namespace math;

float _m[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
constexpr auto iters = 2'000'000;

static void mat4_mult(benchmark::State &state) {
  for (auto _ : state) {    
	for (auto i = 0; i < iters; ++i) {
		mat4 a(_m);
		mat4 b(_m);		
		benchmark::DoNotOptimize(a * b);
	}
  }
}

static void mat4_inv(benchmark::State &state) {
  for (auto _ : state) {	
	for (auto i = 0; i < iters; ++i) {
		mat4 a(_m);
		benchmark::DoNotOptimize(a.Inverse());
	}
  }
}

// Register the function as a benchmark
BENCHMARK(mat4_mult);
BENCHMARK(mat4_inv);

// Run the benchmark
BENCHMARK_MAIN();