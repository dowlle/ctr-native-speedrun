#include <common.h>

void MATH_MatrixMul(MATRIX *output, MATRIX *input, MATRIX *transform)
{
	MatrixRotate(output, input, transform);
	// NOTE(aalhendi): This helper reads and writes only three translation words.
	ApplyMatrixLV_stub((VECTOR *)&transform->t[0], (VECTOR *)&output->t[0]);

	output->t[0] += input->t[0];
	output->t[1] += input->t[1];
	output->t[2] += input->t[2];
}
