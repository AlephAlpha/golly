/*** /
 
 This file is part of Golly, a Game of Life Simulator.
 Copyright (C) 2013 Andrew Trevorrow and Tomas Rokicki.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 
 Web site:  http://sourceforge.net/projects/golly
 Authors:   rokicki@gmail.com  andrew@trevorrow.com
 
 / ***/

#import <UIKit/UIKit.h>

// This view controller is used to display comments in the currently loaded pattern.
// It can also be used (via ShowTextFile) to display the contents of a text file
// and edit that text if the file is located somewhere inside Documents/*.

@interface InfoViewController : UIViewController <UITextViewDelegate, UIGestureRecognizerDelegate>
{
    IBOutlet UITextView *fileView;
    IBOutlet UIBarButtonItem *saveButton;
}

- (IBAction)doCancel:(id)sender;
- (IBAction)doSave:(id)sender;

- (void)saveSucceded:(const char*)savedpath;

@end

// Display the given text file.
void ShowTextFile(const char* filepath, UIViewController* currentView=nil);
