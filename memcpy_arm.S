.global	memcpy_arm_128
memcpy_arm_128:
      PUSH {r3-r10}
1:    PLD [r1, #256]
      VLD1.64 {q0-q1}, [r1,:128]!
      VLD1.64 {q2-q3}, [r1,:128]!
      VLD1.64 {q4-q5}, [r1,:128]!
      VLD1.64 {q6-q7}, [r1,:128]!
      PLD [r1, #384]
      VST1.64 {q0-q1}, [r0,:128]!
      VST1.64 {q2-q3}, [r0,:128]!
      VST1.64 {q4-q5}, [r0,:128]!
      VST1.64 {q6-q7}, [r0,:128]!
      SUBS r2, r2, #128
      BGT 1b
      POP {r3-r10}
