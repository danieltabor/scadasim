$fn = 50;

module panel() {
    cube([198,2,78]);
}

module lcd_hole() {
    translate([0,-1,0])
    cube([99,4,41]);
}

module dsub_hole() {
    translate([0,-1,0])
    union() {
        translate([5.5,0,0])
            cube([20,4,11]);
        translate([1,0,3.25])
            cube([29,4,4.5]);
    }
}

module ps2_hole() {
    translate([0,-1,0]) {
        cube([14,4,13]);
        translate([14+4,0,0])
        cube([14,4,13]);
    }
}

module ps2_shelf() {
    translate([-11,0,-6])
        cube([54,21,2]);
}

module back_panel() {
    difference() {
        panel();
        translate([22,3,8]) {
            translate([0,0,0]) {
                rotate([90,0,0])
                    cylinder(d=5,h=4);
            }
            translate([0,0,61]) {
                rotate([90,0,0])
                    cylinder(d=5,h=4);
            }
            translate([76,0,0]) {
                rotate([90,0,0])
                    cylinder(d=5,h=4);
            }
            translate([76,0,61]) {
                rotate([90,0,0])
                    cylinder(d=5,h=4);
           }
        }
        translate([149,-1.25,10])
            cube([30,2,32]);
        translate([150,-1,10])
            cube([28,4,32]);
            
    }
}

module front_panel() {
    difference() {
        union() {
            panel();
            translate([150,0,10])
                ps2_shelf();
        }
        translate([150,0,10])
            ps2_hole();
        translate([125,0,50])
            rotate([0,90,0])
                dsub_hole();
        translate([170,0,68])
            rotate([0,90,0])
                dsub_hole();
        translate([152,0,68])
            rotate([0,90,0])
                dsub_hole();
        translate([10,0,12])
            lcd_hole();
        
    }
    
}

module bottom_plate() {
    cube([124,170,2]);
}

module front_plate() {
    difference() {
        union() {
            cube([150,2,43]);
            translate([(150.0-124.0)/2.0,0,0])
                cube([124,22+20,2]);
            translate([100,0,20])
                    ps2_shelf();
            translate([100,0,0])
                cube([2,21,15]);
            translate([115,0,0])
                cube([2,21,15]);
            translate([130,0,0])
                cube([2,21,15]);
        }
        translate([100,0,20])
            ps2_hole();
        translate([73,0,38])
            rotate([0,90,0])
                dsub_hole();
        translate([15,0,5])
                dsub_hole();
        translate([15,0,25])
                dsub_hole();
    }
}

module extra_support() {
    difference() {
        cube([4,25,35]);
        translate([-1,25,0])
            rotate([36,0,0])
                cube([6,25,50]);
    }
}

front_plate();
translate([60,2,2])
extra_support();