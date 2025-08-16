#include "bgfx_compute.sh"

//instance data for all instances (pre culling)
BUFFER_RO(instanceDataIn, uint, 0);

// Output
BUFFER_RW(indirectBuffer, uvec4, 1);  // Must be uvec4!!

void main()
{

  // Fill indirect buffer
  uint totalInstances = 0;

  for (int k = 0; k < instanceDataIn[0]; k++) {
        drawIndexedIndirect(
      // Target location params:
            indirectBuffer,             // target buffer
            k,                          // index in buffer
      // Draw call params:
            instanceDataIn[k * 4 + 1],  // number of indices for this draw call
            instanceDataIn[k * 4 + 2],  // number of instances for this draw call. To disable draw call: set to 0
            instanceDataIn[k * 4 + 3],  // offset in the index buffer
            instanceDataIn[k * 4 + 4],  // offset in the vertex buffer. Note that you can use this to "reindex" submeshses - all indicies in this draw will be decremented by this amount
            totalInstances              // offset in the instance buffer. If you are drawing more than 1 instance per call see gpudrivenrendering for how to handle
            );

        totalInstances = totalInstances + instanceDataIn[k * 4 + 2];
    }
}