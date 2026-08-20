/* Regression for hashed preprocessor macro lookup, redefinition, and #undef. */

#define M000 0
#define M001 1
#define M002 2
#define M003 3
#define M004 4
#define M005 5
#define M006 6
#define M007 7
#define M008 8
#define M009 9
#define M010 10
#define M011 11
#define M012 12
#define M013 13
#define M014 14
#define M015 15
#define M016 16
#define M017 17
#define M018 18
#define M019 19
#define M020 20
#define M021 21
#define M022 22
#define M023 23
#define M024 24
#define M025 25
#define M026 26
#define M027 27
#define M028 28
#define M029 29
#define M030 30
#define M031 31
#define M032 32
#define M033 33
#define M034 34
#define M035 35
#define M036 36
#define M037 37
#define M038 38
#define M039 39
#define M040 40
#define M041 41
#define M042 42
#define M043 43
#define M044 44
#define M045 45
#define M046 46
#define M047 47
#define M048 48
#define M049 49
#define M050 50
#define M051 51
#define M052 52
#define M053 53
#define M054 54
#define M055 55
#define M056 56
#define M057 57
#define M058 58
#define M059 59
#define M060 60
#define M061 61
#define M062 62
#define M063 63
#define M064 64
#define M065 65
#define M066 66
#define M067 67
#define M068 68
#define M069 69
#define M070 70
#define M071 71
#define M072 72
#define M073 73
#define M074 74
#define M075 75
#define M076 76
#define M077 77
#define M078 78
#define M079 79
#define M080 80
#define M081 81
#define M082 82
#define M083 83
#define M084 84
#define M085 85
#define M086 86
#define M087 87
#define M088 88
#define M089 89
#define M090 90
#define M091 91
#define M092 92
#define M093 93
#define M094 94
#define M095 95
#define M096 96
#define M097 97
#define M098 98
#define M099 99
#define M100 100
#define M101 101
#define M102 102
#define M103 103
#define M104 104
#define M105 105
#define M106 106
#define M107 107
#define M108 108
#define M109 109
#define M110 110
#define M111 111
#define M112 112
#define M113 113
#define M114 114
#define M115 115
#define M116 116
#define M117 117
#define M118 118
#define M119 119
#define M120 120
#define M121 121
#define M122 122
#define M123 123
#define M124 124
#define M125 125
#define M126 126
#define M127 127
#define M128 128
#define M129 129
#define M130 130
#define M131 131
#define M132 132
#define M133 133
#define M134 134
#define M135 135
#define M136 136
#define M137 137
#define M138 138
#define M139 139
#define M140 140
#define M141 141
#define M142 142
#define M143 143
#define M144 144
#define M145 145
#define M146 146
#define M147 147
#define M148 148
#define M149 149
#define M150 150
#define M151 151
#define M152 152
#define M153 153
#define M154 154
#define M155 155
#define M156 156
#define M157 157
#define M158 158
#define M159 159
#define M160 160
#define M161 161
#define M162 162
#define M163 163
#define M164 164
#define M165 165
#define M166 166
#define M167 167
#define M168 168
#define M169 169
#define M170 170
#define M171 171
#define M172 172
#define M173 173
#define M174 174
#define M175 175
#define M176 176
#define M177 177
#define M178 178
#define M179 179
#define M180 180
#define M181 181
#define M182 182
#define M183 183
#define M184 184
#define M185 185
#define M186 186
#define M187 187
#define M188 188
#define M189 189
#define M190 190
#define M191 191
#define M192 192
#define M193 193
#define M194 194
#define M195 195
#define M196 196
#define M197 197
#define M198 198
#define M199 199
#define PICK M042
#if M042 != 42
#error PICK should expand through hashed lookup
#endif

#define REDEF 1
#undef REDEF
#define REDEF 42

#define FUN(x) ((x) + M040)

int main(void) {
    if (M000 != 0)
        return 1;
    if (M199 != 199)
        return 2;
    if (PICK != 42)
        return 3;
    if (REDEF != 42)
        return 4;
    if (FUN(2) != 42)
        return 5;
    return 42;
}
