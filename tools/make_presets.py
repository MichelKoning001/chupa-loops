# Generates Source/FactoryPresets.cpp  (run: python3 tools/make_presets.py)
# fields: name, pattern, length, repeat, sliceMode, sliceSize, stretch, style, chaos, variation, gate, swing, amount, reverse, octave, fade, sens
# pattern: 0 Free 1 4floor 2 Offbeat 3 Offbeat2x 4 Rolling16 5 KBBB 6 Gallop 7 Broken 8 Random
# length idx: 0=1 1=2 2=4 3=8 4=16 5=32 bars | repeat: 0 off 1=1bar 2=2bars 3=4bars
# sliceMode 0 grid 1 transient | size: 0 1/32 1 1/16 2 1/8 3 1/4 4 1/2 5 1bar | stretch 0 beats 1 smooth | style 0 clean 1 glitch 2 lofi
P = {
"Trance Treats|Trance": [
 ("Sugar Rush Offbeat", 2,2,2,0,1,0,0,25,15,75,0,50,0,0,3,50),
 ("Cherry 138", 2,3,2,0,1,0,0,15,10,85,0,50,0,0,3,50),
 ("Candy Floss Roller", 4,3,2,0,1,0,0,30,20,80,0,50,0,5,3,50),
 ("Gated Gumdrop", 3,2,1,0,1,0,0,20,10,55,0,50,0,0,2,50),
 ("Octave Lollipop", 2,3,2,0,1,0,0,20,20,80,0,50,0,30,3,50),
 ("Double Bubblegum", 3,3,2,0,1,0,0,30,15,70,0,50,0,10,3,50),
 ("Marshmallow Breakdown", 0,4,3,0,3,1,0,20,20,100,0,50,0,0,8,50),
 ("Sherbet Tide", 2,4,3,0,1,0,0,35,25,90,0,50,0,10,4,50),
 ("Mango Glide", 0,3,2,0,2,1,0,25,15,100,0,50,0,0,10,50),
 ("Caramel Nudge", 3,3,2,0,1,0,0,40,30,65,8,50,0,0,3,50)],
"Hard Candy|Hard Trance & Hardhouse": [
 ("Jawbreaker Gallop", 6,2,1,0,1,0,0,20,15,80,0,50,0,0,2,50),
 ("Rock Candy Roller", 4,3,2,0,1,0,0,25,20,70,0,50,0,15,2,50),
 ("Humbug Hammer", 2,2,1,0,1,0,0,15,10,60,0,50,0,0,1.5,50),
 ("Bouncy Gumball", 3,3,2,0,1,0,0,30,20,65,0,50,0,25,2,50),
 ("Jumping Jelly Bean", 6,3,2,0,1,0,0,25,25,75,0,50,0,35,2,50),
 ("Sherbet Dip Pump", 3,2,2,0,1,0,0,20,10,55,0,50,0,0,2,50),
 ("Sour Stabs", 2,3,2,1,1,0,0,35,20,45,0,50,0,10,1.5,60),
 ("Brittle Chop", 7,3,2,0,1,0,1,40,30,70,0,30,5,0,2,50),
 ("Peppermint Drive", 4,4,3,0,1,0,0,30,20,75,0,50,0,5,2,50),
 ("Candy Cane Gallop", 6,4,2,0,1,0,0,45,30,85,0,50,5,20,3,50)],
"Psy Sweets|Psytrance": [
 ("Fizzy KBBB", 5,2,1,0,1,0,0,15,10,80,0,50,0,0,1.5,50),
 ("Mixed Fruit Roller", 5,3,2,0,1,0,0,30,20,85,0,50,0,10,2,50),
 ("Midnight Liquorice Stutter", 5,3,2,0,1,0,1,35,25,75,0,25,0,0,1.5,50),
 ("Coconut Ice Gallop", 6,3,2,0,1,0,0,30,20,80,0,50,0,15,2,50),
 ("Wild Berry Chaos", 5,4,3,1,1,0,0,60,40,70,0,50,10,10,2,65),
 ("Syrup Glide", 5,3,2,0,1,0,0,20,15,95,0,50,0,0,3,50),
 ("Tight Taffy Gate", 5,2,1,0,1,0,0,15,10,50,0,50,0,0,1.5,50),
 ("Rainbow Drop Roll", 5,3,2,0,1,0,0,25,25,80,0,50,0,35,2,50),
 ("Twilight Toffee", 5,3,2,1,1,0,0,40,25,80,0,50,0,5,2,70),
 ("Dark Chocolate 16ths", 4,4,3,0,1,0,0,45,30,70,0,50,5,5,2,50)],
"Techno Toffee|Techno": [
 ("Toffee Rumble", 5,3,2,0,1,0,0,30,20,90,0,50,0,0,6,50),
 ("Peak Time Pear Drops", 4,3,2,0,1,0,0,35,25,65,0,50,0,5,2,50),
 ("Hypno Swirl", 3,4,1,0,1,0,0,20,5,70,0,50,0,0,3,50),
 ("Iron Liquorice Chop", 7,3,2,0,1,0,1,50,30,60,0,45,10,0,2,50),
 ("Mini Mint Stab", 2,3,2,1,1,0,0,40,20,35,0,50,0,0,1.5,60),
 ("Acid Drop", 0,3,2,1,1,0,0,55,30,100,0,50,0,20,2,60),
 ("Dub Fudge Swing", 2,4,3,0,1,1,0,25,20,85,35,50,0,0,8,50),
 ("Broken Barley Sugar", 7,3,2,0,1,0,0,45,30,75,0,50,5,15,2,50),
 ("Nougat Roller", 5,4,3,0,1,0,0,35,20,75,0,50,0,0,2,50),
 ("Salmiak Punch", 3,3,1,0,1,0,2,30,15,60,0,35,0,0,1.5,50)],
"House Candy|House & Garage": [
 ("Honeycomb Swing", 0,3,2,0,2,1,0,25,20,90,40,50,0,0,5,50),
 ("Lemon Drop Skip", 7,3,2,0,1,0,0,35,25,70,55,50,0,10,3,50),
 ("Disco Sprinkles", 3,3,2,0,1,0,0,20,15,70,20,50,0,50,3,50),
 ("Choc Chip Groove", 7,3,2,0,1,0,0,30,20,75,25,50,0,0,3,50),
 ("Funky Fondant Slices", 0,3,2,1,2,0,0,45,25,95,30,50,0,15,4,55),
 ("Classic House Lollipop", 2,2,1,0,1,0,0,15,10,70,30,50,0,0,3,50),
 ("Bass Bonbon Chop", 3,3,2,0,1,0,1,35,25,60,15,30,0,10,2,50),
 ("Soulful Butterscotch Walk", 0,3,2,0,3,1,0,30,25,100,35,50,0,10,8,50),
 ("Jelly Wobble", 6,3,2,0,1,0,0,35,20,70,45,50,5,20,3,50),
 ("Maple Groove", 8,3,2,1,1,0,0,40,25,85,30,50,0,0,4,55)],
"Breakbeat Brittle|Breaks & DnB": [
 ("Peanut Brittle Break", 7,3,2,0,1,0,0,30,20,80,10,50,0,0,3,50),
 ("Marble Candy Roller", 4,4,3,0,1,1,0,30,20,85,0,50,0,0,3,50),
 ("Jungle Gummy Chop", 7,3,1,1,1,0,1,55,35,70,15,40,10,0,2,60),
 ("Nu Skool Candy", 7,3,2,0,1,0,0,40,30,75,5,50,0,15,3,50),
 ("Halftime Hard Boiled", 0,3,2,0,3,1,0,25,20,90,20,50,0,0,6,50),
 ("Molten Sugar Flow", 0,4,3,0,2,1,0,20,15,100,10,50,0,10,10,50),
 ("Neuro Sour Belt", 8,3,2,1,1,0,1,60,40,70,0,60,10,10,1.5,65),
 ("Big Beat Bonbon", 7,2,1,0,1,0,2,30,15,75,20,30,0,0,3,50),
 ("Amen Aniseed Cut", 8,3,1,1,1,0,0,50,30,80,10,50,5,5,2,60),
 ("Two-Step Truffle", 6,3,2,0,1,0,0,30,20,75,40,50,0,0,3,50)],
"Glitch Gummies|Glitch & IDM": [
 ("Gummy Stutter", 4,2,1,0,1,0,1,30,20,90,0,70,0,0,2,50),
 ("Sprinkle Cascade", 0,3,0,0,1,0,1,60,15,100,0,80,10,15,2,50),
 ("Melting Lolly Tape Stop", 2,3,2,0,1,0,1,30,20,90,0,90,0,0,3,50),
 ("Bitshift Bonbon", 8,3,2,1,1,0,1,55,40,80,0,60,5,5,2,60),
 ("Hundreds & Thousands", 0,3,0,0,0,0,1,50,15,100,0,55,5,0,1.5,50),
 ("Robot Rock Candy", 7,3,2,0,1,0,1,60,40,65,10,75,15,20,1.5,50),
 ("Backwards Bubblegum", 0,3,2,0,2,0,1,45,30,100,0,45,45,0,3,50),
 ("Sugar Chaos Theory", 8,4,0,1,1,0,1,85,15,80,0,85,20,25,2,70),
 ("Sugar Crystal Rain", 0,3,0,1,0,0,1,70,15,90,0,65,15,10,2,65),
 ("Popping Candy Glitch", 2,3,2,0,1,0,1,25,20,80,0,50,0,0,2,50)],
"Lo-Fi Liquorice|Lo-Fi": [
 ("Dusty Toffee Tin", 0,3,2,0,2,0,2,25,20,95,25,45,0,0,5,50),
 ("Cassette Caramel", 7,3,2,0,1,0,2,30,20,80,30,35,0,0,4,50),
 ("Crunchy 12-Bit Candy", 3,3,2,0,1,0,2,25,15,70,10,60,0,0,3,50),
 ("Lo-Fi Lollipop", 0,3,2,1,2,1,2,30,20,100,45,50,0,0,6,55),
 ("Vinyl Wine Gum", 2,3,2,0,1,0,2,20,15,80,15,40,0,0,3,50),
 ("Chewed Tape", 7,3,2,0,1,0,2,45,30,75,20,70,10,0,3,50),
 ("Old School Sherbet Crunch", 6,3,2,0,1,0,2,30,20,80,0,55,0,20,3,50),
 ("Bedroom Butterscotch", 8,3,2,1,1,0,2,40,25,85,35,40,5,0,4,55),
 ("Warm Fudge Tape", 0,4,3,0,3,1,2,20,15,100,10,30,0,0,8,50),
 ("8-Bit Arcade Sweets", 4,2,1,0,1,0,2,35,20,60,0,95,0,30,2,50)],
"Rave Candy|Rave Classics": [
 ("Early 90s Candy Rave", 2,3,2,0,1,0,0,30,20,75,0,50,0,20,3,50),
 ("Hardcore Humbug Stab", 3,3,2,1,1,0,0,35,20,50,0,50,0,25,2,60),
 ("Belgian Praline", 4,3,2,0,1,0,0,30,20,70,0,50,0,0,2,50),
 ("Acid Lemon 303", 0,3,2,1,1,0,0,50,30,95,10,50,0,30,2,60),
 ("Happy Sugar Pump", 2,2,1,0,1,0,0,15,10,60,0,50,0,40,2,50),
 ("Bleep & Bonbon", 7,3,2,0,1,0,0,35,25,70,10,50,0,0,3,50),
 ("Gabber Gumball", 6,2,1,0,1,0,2,20,10,60,0,40,0,0,1.5,50),
 ("Candy Warehouse 1991", 1,3,2,0,1,0,2,25,20,80,0,25,0,10,3,50),
 ("Big Lolly Anthem", 3,4,3,0,1,0,0,30,20,70,0,50,5,30,3,50),
 ("Rainbow Rave Roller", 4,4,3,0,1,0,0,35,25,75,0,50,0,20,2,50)],
"Pick 'n' Mix|Experimental": [
 ("Random Pick \'n\' Mix", 8,4,0,0,1,0,0,60,15,85,0,50,5,5,3,50),
 ("Total Sugar Meltdown", 8,3,0,1,0,0,1,100,15,90,20,100,30,30,1.5,70),
 ("Backwards Lolly", 0,3,2,0,2,0,0,40,30,100,0,50,70,0,4,50),
 ("Helium Gumdrops", 4,3,1,0,1,0,0,40,30,70,0,50,0,80,2,50),
 ("Slow Melt 32", 0,5,3,0,3,1,0,30,40,100,0,50,10,10,12,50),
 ("Jelly Bean Hunter", 0,3,2,1,1,0,0,70,40,90,0,50,0,0,2,80),
 ("Swinging Candy Lab", 7,3,2,0,1,0,0,35,25,75,75,50,0,0,3,50),
 ("Short & Sour", 4,2,1,0,1,0,0,30,20,25,0,50,0,0,1,50),
 ("Long Taffy Pull", 0,4,3,0,4,1,0,25,20,100,0,50,0,0,25,50),
 ("Mirror Mint Lo-Fi", 0,3,2,0,2,0,2,50,30,100,20,70,50,15,5,50)],
}
keys = ["pattern","length","motif","sliceMode","sliceSize","stretch","style","chaos","variation","gate","swing","amount","reverse","octave","fade","sensitivity"]
out = ['// Generated by tools/make_presets.py - do not edit by hand', '#include "Presets.h"', '', 'namespace slicetribe', '{', '',
       'const std::vector<PresetData>& factoryPresets()', '{', '    static const std::vector<PresetData> presets = []', '    {', '        std::vector<PresetData> v;',
       '        auto add = [&v] (const char* cat, const char* genre, const char* name, std::initializer_list<float> x)', '        {',
       '            static const char* ids[] = { ' + ', '.join('"%s"' % k for k in keys) + ' };',
       '            PresetData p; p.category = cat; p.genre = genre; p.name = name; p.factory = true;',
       '            int i = 0; for (float f : x) p.values[ids[i++]] = f;', '            v.push_back (p);', '        };',
       '        add ("Init", "", "Init", { 0, 2, 2, 0, 2, 0, 0, 30, 15, 100, 0, 50, 0, 0, 3, 50 });']
n = 0
for cat, lst in P.items():
    assert len(lst) == 10, cat
    c, genre = cat.split('|')
    for t in lst:
        name = t[0]; vals = t[1:]
        assert len(vals) == 16, name
        pat,ln,mo,sm,sz,stv,sty = vals[:7]
        assert 0<=pat<=8 and 0<=ln<=5 and 0<=mo<=3 and sm in (0,1) and 0<=sz<=5 and stv in (0,1) and 0<=sty<=2, name
        ch,va,ga,sw,am,rv,oc,fa,se = vals[7:]
        assert 10<=ga<=100 and 1<=fa<=25 and all(0<=q<=100 for q in (ch,va,sw,am,rv,oc,se)), name
        esc = lambda t: t.replace('\\', '\\\\').replace('"', '\\"')
        out.append('        add ("%s", "%s", "%s", { %s });' % (esc(c), esc(genre), esc(name), ', '.join(str(x) for x in vals)))
        n += 1
out += ['        return v;', '    }();', '    return presets;', '}', '', '} // namespace slicetribe', '']
names=[t[0] for l in P.values() for t in l]
assert len(set(names))==len(names), "duplicate names"
open('Source/FactoryPresets.cpp','w').write('\n'.join(out))
print(n, "factory presets")
