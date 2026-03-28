%Parameter definition for our UGV model
rL=0.052959;    %Radius of left wheel [m]
rR=0.052959;    %Radius of right wheel [m]
b=0.5842;       %Effective width of UGV [m]

Vmax=0.44;      %Max translational speed [m/s]
wmax=Vmax/rR;   %Max angular speed [rad/s]



%%%%%%%%%%%%%%%%%%%%%%% Way Points %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
X_array  = [5 5 0]; 
Y_array  = [-3 3 0]; 

%%%%%%%%%%%%%%%%%%% guidance parameter %%%%%%%%%%%%%%%%%%%%%%%%%%%
rp1 =  0.3; % 1 -> [m] proximity circle to start slowing down
rp2 =  0.5;  % [m] radius of wayPoint proximity circle to switch to the next wayPoint