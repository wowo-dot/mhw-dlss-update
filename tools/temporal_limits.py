"""Versioned capture sizes; never infer limits from untrusted array lengths."""
def frame_count(header):
    return 9 if header['version'] in ('1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 3
