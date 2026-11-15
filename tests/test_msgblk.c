// SPDX-License-Identifier: GPL-2.0-or-later
/* Copyright (C) 2026 Thalles Campagnani */
/* Message block layout vs. the facts stated in docsClean/05 and /09. */
#include "check.h"
#include "common/msgblk_fields.h"

static const struct { const char *n; unsigned off, sz, dir, sgn; } f[] = {
#define X(name, off, sz, dir, sgn) { #name, off, sz, dir, sgn },
	MB_FIELD_LIST(X)
#undef X
};

int main(void)
{
	unsigned i, end = 0;

	CHECK(sizeof(f) / sizeof(f[0]) == MB_NFIELDS);
	for (i = 0; i < MB_NFIELDS; i++) {
		CHECK(f[i].off == end);
		end = f[i].off + f[i].sz;
	}
	CHECK(end == MB_SIZE);

	/* docsClean/05 core control fields */
	CHECK(MB_MsgMisc == 0x001 && MB_PmuRevision == 0x002 && MB_PmuRevision_SZ == 2);
	CHECK(MB_DRAMFreq == 0x006 && MB_DRAMFreq_SZ == 2);
	CHECK(MB_DfiFreqRatio == 0x008 && MB_SequenceCtrl == 0x010);
	CHECK(MB_HdtCtrl == 0x012 && MB_CsTestFail == 0x00f && MB_CsTestFail_DIR == MB_OUT);
	CHECK(MB_Quickboot == 0x019 && MB_X8Mode == 0x01c);
	CHECK(MB_EnabledDQsChA == 0x024 && MB_CsPresentChA == 0x025);
	CHECK(MB_EnabledDQsChB == 0x03f && MB_CsPresentChB == 0x040);
	/* trained outputs and quickboot MR shadows (docsClean/11) */
	CHECK(MB_TrainedVREFCA_A0 == 0x033 && MB_TrainedVREFDQ_B1 == 0x051);
	CHECK(MB_RxClkDly_Margin_A0 == 0x037 && MB_DeviceVref_Margin_B1 == 0x059);
	CHECK(MB_QBPllUPllProg0 == 0x130 && MB_QBPllCtrl5 == 0x13c);
	CHECK(MB_Disable2D == 0x0ce);
	CHECK_DONE("test_msgblk");
}
