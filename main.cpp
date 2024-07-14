#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <iostream>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION 
#include <stb_image_write.h>



void DistanceTransform(GLuint, int, int, GLuint[2]);
void Diffusion(GLuint, GLuint, GLuint, int, int, int);
static unsigned int CompileShader(unsigned int type, const std::string& source);
static unsigned int CreateShader(const std::string& vert, const std::string& frag);
void renderQuad();

int main(int argc, char *argv[]){
	if(argc != 3){
		std::cout << "Usage: ./OpenGlDiff /path/to/file.png iterations"<< std::endl;
		return 0;
	}
	//load image
	int width, height, channels;		
	unsigned char* imageData = stbi_load(argv[1],&width,&height,&channels, 0);	
	//unsigned char* imageData = stbi_load("./input/crab.png",&width,&height,&channels, 0);
	if(imageData == NULL) std::cout << "failed to load image\n";

	//intialization stuff
	if (!glfwInit()) std::cerr << "Failed to initialize GLFW" << std::endl; 
       	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(width,height, "Invisible Window", nullptr, nullptr);
	if (!window) std::cerr << "Failed to create GLFW window" << std::endl;      
	glfwMakeContextCurrent(window);	
	if(glewInit() != GLEW_OK) std::cout << "failed to start glew";

	//generate and configure texture for image
	GLuint image;
	glGenTextures(1, &image);
	glBindTexture(GL_TEXTURE_2D, image); 

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//allocate memory 
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
	 


	//create the two position textures	
	GLuint posTex0;
	glGenTextures(1,&posTex0);
	glBindTexture(GL_TEXTURE_2D, posTex0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, width, height, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, NULL);

	GLuint posTex1;
	glGenTextures(1,&posTex1);	
	glBindTexture(GL_TEXTURE_2D, posTex1);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, width, height, 0, GL_RG_INTEGER, GL_UNSIGNED_SHORT, NULL);
	GLuint posTex[2] = {posTex0, posTex1};
	

	DistanceTransform(image, width, height, posTex);


	//create a swap texture
	GLuint swapTex;
	glGenTextures(1, &swapTex);
	glBindTexture(GL_TEXTURE_2D, swapTex); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);


	int iterations = std::stoi(argv[2]);
	Diffusion(image, posTex[0], swapTex, width, height, iterations);

	
	//read pixels from the texture
    	unsigned char* pixels = new unsigned char[width * height * 4];
    	glBindTexture(GL_TEXTURE_2D, swapTex);
    	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,  pixels);	
	stbi_write_png("out.png", width, height, channels,/*data*/pixels, width*channels);

	//cleanup
	glDeleteTextures(1,&image);
	glDeleteTextures(1,&posTex0);
	glDeleteTextures(1,&posTex1);	
	glDeleteTextures(1,&swapTex);
	stbi_image_free(imageData);
	delete[] pixels;
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}



void DistanceTransform(GLuint image, int width, int height, GLuint posTex[2]){	
	
	//initialize framebuffer
	GLuint FBO;		
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER,FBO);
	//attach texture
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, posTex[0], 0);
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)std::cout <<"Framebuffer Fail: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::endl;


	std::string identityVS = R"(
#version 330 core

layout(location = 0)in vec2 vPos; 

void main() {
    gl_Position = vec4(vPos, 0.0,1.0);    
})";

	std::string inputFS = R"(
#version 330 core

out uvec2 position;
uniform sampler2D imageTexture;

void main() {
	float mask = texelFetch(imageTexture, ivec2(gl_FragCoord.xy), 0).a;
	position = (mask > 0.0) ? uvec2(gl_FragCoord.xy): uvec2(-1.0, -1.0); 

})";
	static unsigned int inputToPosition = CreateShader(identityVS, inputFS);
	//initialization shader
	glUseProgram(inputToPosition);	

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, image);
	glUniform1i(glGetUniformLocation(inputToPosition, "imageTexture"), 0);	

	renderQuad();			


	std::string sweepVS = R"(
#version 330 core

uniform vec2 translation;
in vec2 position;

void main() {
	gl_Position.xy = position + translation; 
	gl_Position.w = 1.0;
})";

	std::string sweepFS = R"(
#version 330 core

uniform usampler2D positionMap;
uniform ivec2 offset[3];

out uvec2 position;

void main() {
    ivec2 size = textureSize(positionMap, 0).xy;
    position = texelFetch(positionMap, ivec2(gl_FragCoord.xy), 0).xy;
    float distance = length(vec2(position.xy)-gl_FragCoord.xy);

    for(int i = 0; i < 3; ++i) {
        ivec2 address = ivec2(gl_FragCoord.xy)+offset[i];
        if(address.x < 0 || address.y < 0 || address.x >= size.x || address.y >= size.y)
            continue;
        uvec2 neighborPos = texelFetch(positionMap, address, 0).xy;
        vec2 diff = vec2(neighborPos.xy)-gl_FragCoord.xy;
        float neighborDist = length(diff);
        if(neighborDist < distance) {
            distance = neighborDist;
            position = neighborPos;
        }
    }
    
}
)";
	//for the main vertical and horizontal line sweep in the big for loop
	static unsigned int sweep = CreateShader(sweepVS, sweepFS);	


std::string combineFS = R"(
#version 330 core


uniform usampler2D positionMap;
uniform ivec2 offset;

out uvec2 position;

void main() {
    if(int(gl_FragCoord.x)%2 == offset.x || int(gl_FragCoord.y)%2 == offset.y)
        discard;
    position = texelFetch(positionMap, ivec2(gl_FragCoord.xy), 0).xy;
})";
	//cleans up the texture after each sweep ?
	static unsigned int combine = CreateShader(identityVS, combineFS);	

	

	//set up the two lines
	GLuint horizontalLineBuffer, verticalLineBuffer;

	glGenBuffers(1, &horizontalLineBuffer);
	glGenBuffers(1, &verticalLineBuffer);

	glBindBuffer(GL_ARRAY_BUFFER, horizontalLineBuffer);
	float horizontalLine[] = {-1.0f, 0.0f, 1.0f, 0.0f};
	glBufferData(GL_ARRAY_BUFFER, 4*sizeof(float), horizontalLine, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, verticalLineBuffer);
	float verticalLine[] = {0.0f, -1.0f, 0.0f, 1.0f};
	glBufferData(GL_ARRAY_BUFFER, 4*sizeof(float), verticalLine, GL_STATIC_DRAW);


	int sweepOffsets[4][6] = {
		{-1,-1,-1,0,-1,1},
		{1,-1,1,0,1,1},
		{-1,-1,0,-1,1,-1},
		{-1,1,0,1,1,1},
	};


	for(int j = 0; j < 4; j++){	
		if(j%2==0){
			glBindTexture(GL_TEXTURE_2D, posTex[1]);
			glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RG16UI, 0,0,width,height,0);
		}

		glUseProgram(sweep);
		glUniform2i(glGetUniformLocation(sweep, "offset[0]"), sweepOffsets[j][0], sweepOffsets[j][1]);
		glUniform2i(glGetUniformLocation(sweep, "offset[1]"), sweepOffsets[j][2], sweepOffsets[j][3]);
		glUniform2i(glGetUniformLocation(sweep, "offset[2]"), sweepOffsets[j][4], sweepOffsets[j][5]);
		int length = (j<2) ? width : height;	
		
		for(int i = 0; i < length; i++){
			int textureIndex = i%2;
			float translation = (j%2 == 0) ? ((i+0.5)/length*2-1) : ((length-i-0.5)/length*2-1);
			if(j<2) glUniform2f(glGetUniformLocation(sweep, "translation"), translation,  0);	
			else glUniform2f(glGetUniformLocation(sweep, "translation"), 0, translation);	

			glBindTexture(GL_TEXTURE_2D, posTex[textureIndex]); 
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, posTex[1-textureIndex], 0);
			if(j<2) glBindBuffer(GL_ARRAY_BUFFER, verticalLineBuffer);
			else glBindBuffer(GL_ARRAY_BUFFER, horizontalLineBuffer);
			//draw lines
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);	
			glDrawArrays(GL_LINES, 0, 2);			
						
		}	
	
				
		glUseProgram(combine);	
		if(j < 2){	
			glUniform2i(glGetUniformLocation(combine, "offset"), width%2, 2);  
		}else{
		
			glUniform2i(glGetUniformLocation(combine, "offset"), 2, height%2);  
		}
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, posTex[1]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, posTex[0], 0);	
		renderQuad();	
	
	}
	//framebuffer must be unbound to render any texture to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	std::string fetchFS = R"(
#version 330 core

uniform usampler2D positionMap;
uniform sampler2D imageTexture;

out vec4 FragColor;

void main() {
	ivec2 address = ivec2(texelFetch(positionMap, ivec2(gl_FragCoord.xy), 0).xy);	
	FragColor = texelFetch(imageTexture, address, 0);	
})";
	static unsigned int fetch  = CreateShader(identityVS, fetchFS);	


	glUseProgram(fetch);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, image);	
	glUniform1i(glGetUniformLocation(fetch, "imageTexture"), 1);		

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, posTex[0]);	
	glUniform1i(glGetUniformLocation(fetch, "positionMap"), 0);		

	renderQuad();

	//relative RGBA values from position map are put into original image texture
	glBindTexture(GL_TEXTURE_2D, image);	
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0,width, height, 0);

	
	glDeleteFramebuffers(1, &FBO);
	glDeleteFramebuffers(1, &horizontalLineBuffer);
	glDeleteFramebuffers(1, &verticalLineBuffer);
	
	
}


void Diffusion(GLuint image, GLuint posMap, GLuint swapTex, int width, int height, int iterations){

	GLuint FBO;
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	std::string identityVS = R"(
#version 330 core
layout (location = 0) in vec2 vPos;  

void main() {
    gl_Position = vec4(vPos, 0.0,1.0);    
})";
	std::string diffusionFS = R"(
#version 330 core
uniform float factor;
uniform sampler2D imageTexture;
uniform usampler2D positionMap;

out vec4 color;

const ivec2 offset[4] = ivec2[](
    ivec2(-1, 0),
    ivec2(1, 0),
    ivec2(0, -1),
    ivec2(0, 1)
);

void main() {
    ivec2 size = textureSize(positionMap, 0).xy;
    vec2 position = vec2(texelFetch(positionMap, ivec2(gl_FragCoord.xy), 0).xy); 
    float distance = max(0.0, length(vec2(position)-gl_FragCoord.xy)-0.5)*factor; 
    
    for(int i = 0; i < 4; ++i)
	 color += texture(imageTexture, (gl_FragCoord.xy+vec2(offset[i])*distance)/vec2(size.xy));
    color *= 0.25;

        
})";	
	//retrieves color based off distances from the position map and color of neighboring pixels
	static unsigned int diffusion = CreateShader(identityVS, diffusionFS);
	
	glUseProgram(diffusion);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, posMap);
	glUniform1i(glGetUniformLocation(diffusion, "positionMap"), 1);		

	glActiveTexture(GL_TEXTURE0);
	for(int i = 0; i < iterations; i++){
		if(i == iterations - 1) glBindFramebuffer(GL_FRAMEBUFFER, 0);	
		else{
			if(i%2==0) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, swapTex, 0);	
			else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);	
		}	
		if(i%2==0) glBindTexture(GL_TEXTURE_2D, image);
		else glBindTexture(GL_TEXTURE_2D, swapTex);	
		glUniform1i(glGetUniformLocation(diffusion, "imageTexture"), 0);		
		glUniform1f(glGetUniformLocation(diffusion, "factor"), 0.92387*(1-i/iterations));
		renderQuad();	
	}
	
	glBindTexture(GL_TEXTURE_2D, swapTex);	
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0,width, height, 0);

	glDeleteFramebuffers(1,&FBO);	
}

static unsigned int CompileShader(unsigned int type, const std::string& source){
	unsigned int id = glCreateShader(type);
	const char* src = source.c_str();
	glShaderSource(id, 1, &src, nullptr);
	glCompileShader(id);
	
	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if(!result){
		int length;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		char* message= (char*)alloca(length*sizeof(char));
		glGetShaderInfoLog(id, length, &length, message);
		std::cout << "Failed to compile: " << type <<": "<< message << std::endl;
		glDeleteShader(id);
		return -1;
	}


	return id;
}

static unsigned int CreateShader(const std::string& vert, const std::string& frag){
	unsigned int program = glCreateProgram();
	unsigned int vs = CompileShader(GL_VERTEX_SHADER, vert);
	unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, frag);

	glAttachShader(program,vs);
	glAttachShader(program,fs);
	glLinkProgram(program);
	glValidateProgram(program);

	glDeleteShader(vs);
	glDeleteShader(fs);
	return program;
}

void renderQuad(){
    //draws two triangles to cover the texture 
    float vertices[] = {     
        -1.0f, -1.0f,    
         1.0f, -1.0f,   
         1.0f,  1.0f,   

        -1.0f, -1.0f,   
         1.0f,  1.0f,    
        -1.0f,  1.0f,    
    };

    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 12*sizeof(float), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES,0,6);
 
    glDeleteBuffers(1, &VBO);
}
