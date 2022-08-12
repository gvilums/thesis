/**
 * app.c
 * GEMV Host Application Source File
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dpu.h>
#include <dpu_log.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#if ENERGY
#include <dpu_probe.h>
#endif

#include "../support/common.h"
#include "../support/params.h"
#include "../../common/timer.h"

// Define the DPU Binary path as DPU_BINARY here
#ifndef DPU_BINARY
#define DPU_BINARY "./bin/gemv_dpu"
#endif

static T* A;
static T* B;
static T* C;
static T* C_dpu;

// Create input arrays
static void init_data(T* A, T* B, unsigned int m_size, unsigned int n_size) {
	srand(0);

	for (unsigned int i = 0; i < m_size * n_size; i++)
	{
		A[i] = (unsigned int) (rand()%50);
	}

	for (unsigned int i = 0; i < n_size; i++)
	{
		B[i] = (unsigned int) (rand()%50);
	}
}

// Compute output in the host
static void gemv_host(T* C, T* A, T* B, unsigned int m_size, unsigned int n_size) {
	for (unsigned int i = 0; i < m_size; i++)
	{
		C[i] = 0;
	}

	for (unsigned int m = 0; m < m_size; m++) {
		for (unsigned int n = 0; n < n_size; n++)
		{
			C[m] += A[m * n_size + n] * B[n];
		}
	}
}

// Main of the Host Application
int main(int argc, char **argv) {

	struct Params p = input_params(argc, argv);

	struct dpu_set_t dpu_set, dpu;
	uint32_t nr_of_dpus;

	// Allocate DPUs and load binary
	DPU_ASSERT(dpu_alloc(DPU_ALLOCATE_ALL, NULL, &dpu_set));
	DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
	DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &nr_of_dpus));

	unsigned int i;
	unsigned int m_size = p.m_size;
	unsigned int n_size = p.n_size;

	// Initialize help data
	dpu_info = (struct dpu_info_t *) malloc(nr_of_dpus * sizeof(struct dpu_info_t));
	dpu_arguments_t *input_args = (dpu_arguments_t *) malloc(nr_of_dpus * sizeof(dpu_arguments_t));
	uint32_t max_rows_per_dpu = 0;
	uint32_t n_size_pad = n_size;
	if(n_size % 2 == 1)
	{
		n_size_pad++;
	}

	i = 0;
	DPU_FOREACH(dpu_set, dpu, i) {
		uint32_t rows_per_dpu;
		uint32_t prev_rows_dpu = 0;
		uint32_t chunks = m_size / nr_of_dpus;
		rows_per_dpu = chunks;
		uint32_t rest_rows = m_size % nr_of_dpus;
		if (i < rest_rows)
			rows_per_dpu++;
		if (rest_rows > 0) {
			if (i >= rest_rows)
				prev_rows_dpu = rest_rows * (chunks + 1) + (i - rest_rows) * chunks;
			else
				prev_rows_dpu = i * (chunks + 1);
		} else {
			prev_rows_dpu = i * chunks;
		}

		// Keep max rows for parallel transfers
		uint32_t rows_per_dpu_pad = rows_per_dpu;
		if (rows_per_dpu_pad % 2 == 1) // 4-byte elements
			rows_per_dpu_pad++;
		if (rows_per_dpu_pad > max_rows_per_dpu)
			max_rows_per_dpu = rows_per_dpu_pad;

		dpu_info[i].rows_per_dpu = rows_per_dpu;
		dpu_info[i].rows_per_dpu_pad = rows_per_dpu_pad;
		dpu_info[i].prev_rows_dpu = prev_rows_dpu;

		// Copy input arguments to DPU
		input_args[i].n_size = n_size;
		input_args[i].n_size_pad = n_size_pad;
		input_args[i].nr_rows = rows_per_dpu;
	}

	A = malloc(max_rows_per_dpu * nr_of_dpus * n_size_pad * sizeof(T));
	B = malloc(n_size_pad * sizeof(T));
	C = malloc(max_rows_per_dpu * nr_of_dpus * sizeof(T));

	// Initialize data with arbitrary data
	init_data(A, B, m_size, n_size);


	// Compute output on CPU (performance comparison and verification purposes)
	for (unsigned int rep = 0; rep < ITERATIONS; rep++) {



		timer_start_transfer();
		// Input arguments
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			// Copy input arguments to DPU
			input_args[i].max_rows = max_rows_per_dpu;

			DPU_ASSERT(dpu_prepare_xfer(dpu, input_args + i));
		}

		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, "DPU_INPUT_ARGUMENTS", 0, sizeof(dpu_arguments_t), DPU_XFER_DEFAULT));

		// Copy input array and vector
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, A + dpu_info[i].prev_rows_dpu * n_size));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, 0, max_rows_per_dpu * n_size_pad * sizeof(T), DPU_XFER_DEFAULT));
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, B));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_TO_DPU, DPU_MRAM_HEAP_POINTER_NAME, max_rows_per_dpu * n_size_pad * sizeof(T) , n_size_pad * sizeof(T), DPU_XFER_DEFAULT));

		timer_launch_dpus();

		DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));

		timer_retrieve_data();

		// Retrieve results
		C_dpu = malloc(max_rows_per_dpu * nr_of_dpus * sizeof(T));
		i = 0;
		DPU_FOREACH(dpu_set, dpu, i) {
			DPU_ASSERT(dpu_prepare_xfer(dpu, C_dpu + i * max_rows_per_dpu));
		}
		DPU_ASSERT(dpu_push_xfer(dpu_set, DPU_XFER_FROM_DPU, DPU_MRAM_HEAP_POINTER_NAME, max_rows_per_dpu * n_size_pad * sizeof(T) + n_size_pad * sizeof(T), max_rows_per_dpu * sizeof(T), DPU_XFER_DEFAULT));
		timer_start_combine();
		timer_finish();
	}

	printf("matmul, ");
	timer_print_summary();

	// Deallocation
	free(A);
	free(B);
	free(C);
	free(C_dpu);
	DPU_ASSERT(dpu_free(dpu_set));

	return 0;
}
