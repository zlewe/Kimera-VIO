%
R_0=eye(3)*1;
R_x=rotx(30);
R_y=roty(-30);
R_z=rotz(30);

Ref=R_0;

ref_frame(Ref)
lighting phong; 
camlight left;
coloryellow=['y','y','y'];

ref_frame_grey(Ref,[1.5,0,0])
Ref=R_x*Ref;
ref_frame(Ref,[1.5,0,0],coloryellow)

ref_frame_grey(Ref,[3,0,0])
Ref=R_y*Ref;
ref_frame(Ref,[3,0,0],coloryellow)

ref_frame_grey(Ref,[4.5,0,0])
Ref=R_z*Ref;
ref_frame(Ref,[4.5,0,0],coloryellow)

ref_frame(Ref,[6,0,0])


