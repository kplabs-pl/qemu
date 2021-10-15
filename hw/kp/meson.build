specific_ss.add(when: 'CONFIG_POSIX', if_true: files('memfile.c'))
specific_ss.add(when: 'CONFIG_WIN32', if_true: files('memfile.c'))

specific_ss.add(when: 'CONFIG_KP_ARGS_DEVICE', if_true: files('args-device.c'))

specific_ss.add(when: 'CONFIG_KP_EXIT_DEVICE', if_true: files('exit-device.c'))

edi_ss = ss.source_set()
edi_ss.add(files(
  'edi-commands.c',
  'edi-list.c',
  'edi-group.c',
  'edi-regs.c',
  'edi.c',
))

arm_softmmu_ss = ss.source_set()
arm_softmmu_ss.add(when: 'CONFIG_KP_EDI', if_true: files('edi-arm.c'))
arm_softmmu_ss.add_all(when: 'CONFIG_KP_EDI', if_true: edi_ss)

avr_softmmu_ss = ss.source_set()
avr_softmmu_ss.add(when: 'CONFIG_KP_EDI', if_true: files('edi-avr.c'))
avr_softmmu_ss.add_all(when: 'CONFIG_KP_EDI', if_true: edi_ss)

hw_arch['arm'].add_all(arm_softmmu_ss)
hw_arch['avr'].add_all(avr_softmmu_ss)