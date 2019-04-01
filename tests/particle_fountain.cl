__kernel void particle_fountain(__global float4* position, 
                                            __global float4* color, 
                                            __global float4* velocity, 
                                            __global float4* start_position, 
                                            __global float4* start_velocity, 
                                            float time_step)
{
    unsigned int i = get_global_id(0);
    float4 p = position[i];
    float4 v = velocity[i];
    float life = velocity[i].w;
    life -= time_step;
    if (life <= 0.f)
    {
        p = start_position[i];
        v = start_velocity[i];
        life = 1.0f;    
    }
    v.z -= 9.8f*time_step;
    p.x += v.x*time_step;
    p.y += v.y*time_step;
    p.z += v.z*time_step;
    v.w = life;
    position[i] = p;
    velocity[i] = v;
    color[i].w = life; /* Fade points as life decreases */
}