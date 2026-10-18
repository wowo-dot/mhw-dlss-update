EXTERN mhw_temporal_dispatch:PROC
EXTERN mhw_temporal_consume:PROC


INCLUDE native_frame.inc
.code
NATIVE_ENTRY temporal_dispatch_entry, mhw_temporal_dispatch, 2
NATIVE_ENTRY temporal_consumer_entry, mhw_temporal_consume, 0
END
