; Extra ZP variables for neslib/crt0.s
; display.sinc imports _oam_off but neslib/crt0.s doesn't define it

.segment "ZEROPAGE"

.global _oam_off
_oam_off: .res 1