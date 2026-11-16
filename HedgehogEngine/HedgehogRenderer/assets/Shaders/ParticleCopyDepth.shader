pipeline_layout: ../Pipelines/ParticleCompute.pl

shaders:
  - stage: compute
    path: Particles/CopyDepth.comp.spv
