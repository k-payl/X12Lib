## Constant buffer workflows:
1) __Rare updates__ (manual): Create separate ICoreBuffer with BUFFER_FLAGS::GPU_READ flags (fast GPU access). For common engine parameters, settings, viewport... Further updates are made through __ICoreBuffer::SetData()__
2) __Per-frame updates__: Create separate ICoreBuffer with BUFFER_FLAGS::CPU_WRITE flags (fast uploading). For camera MVP matrix, positions... Further updates are made through __ICoreBuffer::SetData()__
3) __Per-draw updates__: No need create separate buffer. Send to creation shader options { "[constant buffer name]", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW}. Then update constant buffer through __CommandContext::UpdateInlineConstantBuffer()__