; dev -q -X -o -Tahiti -ns -nt -rawdump -save_token_file copy_timestamp_query_pool.il

il_cs_2_0

dcl_num_thread_per_group 64, 1, 1

dcl_literal l0, 0x3, 0x00000001, 0x00000002, 0x1
dcl_literal l1, 0x8, 0x4, 0, 0

; 64-bit source timestamp buffer uav
dcl_uav_id(0)_type(buffer)_fmtx(uint)

; destination buffer uav
dcl_raw_uav_id(1)

; { query_count, flags, dst_stride, start_query }
dcl_cb cb0[1]

; check if thread ID is in copy range.
ult r0.x, vAbsTid0.x, cb0[0].x

if_logicalnz r0.x
    ; compute timestamp 64-bit element offset from thread_id + start_query
    uadd r0.x, vAbsTid0.x, cb0[0].w

    ; 64-bit read of timestamp counter
    uav_load_id(0) r1.xy__, r0.x

    ; determine if 64-bit results are required
    iand r2.x, cb0[0].y, l0.y

    ; determine if availability needs to be written
    iand r2.y, cb0[0].y, l0.z

    ; compute start of destination query result array
    umul r3.x, vAbsTid0.x, cb0[0].z

    ; determine if counter is non-zero and produce a 0/1 availability value (note that
    ; we could also just output any non-zero value for available, but we are being "pretty" here)
    ior r1.z, r1.x, r1.y
    if_logicalnz r1.z
        mov r1.z, l0.w ; 64-bit low word
    endif
    mov r1.w, r1.0 ; 64-bit high word

    ; if results are requested in 64-bit
    if_logicalnz r2.x
        ; write counter value only if result is available to first element
        if_logicalnz r1.z
            uav_raw_store_id(1) mem.xy__, r3.x, r1.xyyy
        endif

        ; write availability if it needs to be written to second element
        if_logicalnz r2.y
            uadd r3.y, r3.x, l1.x
            uav_raw_store_id(1) mem.xy__, r3.y, r1.zwww
        endif
    else ; results are requested in 32-bit
        ; write counter value only if result is available to first element
        if_logicalnz r1.z
            uav_raw_store_id(1) mem.x___, r3.x, r1.xxxx
        endif

        ; write availability if it needs to be written to second element
        if_logicalnz r2.y
            uadd r3.y, r3.x, l1.y
            uav_raw_store_id(1) mem.x___, r3.y, r0.zzzz
        endif
    endif
endif
